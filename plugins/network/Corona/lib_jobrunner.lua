--====================================================================--
-- Module: jobrunner
--====================================================================--
--
local M = {}

local log = require("lib_log")
local logger = log.getModuleLogger("JOBRUNNER")

-----------------------------------------------------------------------------
-- Async job runner
--
--    Usage:
--    ----------------------------------------------------
--
--    local function onStartJob( jobState, onComplete )
--        -- You must use the supplied onComplete as the completion handler for
--        -- your async process.  Any params passed to this handler will be
--        -- forwarded to the onFinish handler that you supply when registering
--        -- the job.
--        --
--        timer.performWithDelay(1000, onComplete)
--    end
--    
--    local function onFinishJob( jobState, ... )
--        return true -- If job runner should run next job
--    end
--    
--    local function onCancelJob( jobState )
--        return true -- If job successfully cancelled
--    end
--
--    local function onJobEvent( event )
--        event.lastJobState, event.nextJobState, event.retryCount
--        if event.phase == "began" then
--             print("began")
--        elseif event.phase == "runjob" then
--             print("runjob")
--        elseif event.phase == "ended" then
--             print("ended")
--        end
--    end
--
-----------------------------------------------------------------------------
--
-- !!! We could really use a timeout per job also to terminate (and potentially retry) non-productive jobs
--
function M.createAsyncJobRunner( )
    local asyncJobRunner = {
        eventListener = nil,
        currentJobIndex = nil,
        currentJob = nil,
        previousJob = nil,
        currentRetryCount = 0,
        jobs = {},
    }   

    function asyncJobRunner:onStartCurrentJob( )
        logger:debug("Start job")
        self.previousJob = self.currentJob
        self.currentJob = self.jobs[self.currentJobIndex]
        if self.eventListener then
            self.eventListener(
                {
                    phase = "runjob", 
                    previousJob = self.previousJob and self.previousJob.jobState, 
                    nextJob = self.currentJob.jobState,
                    retryCount = self.currentRetryCount,
                }
            )
        end
        local started = self.currentJob.onStartJob(self.currentJob.jobState, function(...) self:onFinishCurrentJob(unpack(arg)) end)
        if started then
            logger:debug("Job runner, job started")
            return true
        else
            logger:debug("Job runner, job failed to start")
            return false
        end
    end
    
    function asyncJobRunner:onStartNextJob( )
        logger:debug("Start next job")
        self.currentRetryCount = 0
        local jobIndex = (self.currentJobIndex or 0) + 1
        if jobIndex <= #self.jobs then
            self.currentJobIndex = jobIndex
            self:onStartCurrentJob()
        else
            logger:debug("Job runner, no more jobs")
            self.currentJobIndex = nil
            if self.eventListener then
                self.eventListener({phase = "runjob", previousJob = self.currentJob.jobState})
                self.eventListener({phase = "ended"})
            end
            return false
        end
    end
    
    -- !!! We really need to protect against this getting called by a job more than once, because all hell can break loose.
    --     Particulary if it gets called after the end of jobs, in which case the onStartNextJob will start over because
    --     he currentJobIndex has been reset.
    --
    --     For each job, maybe have a "finished" flag that gets set here and prevents multiple finishes (and gets reset
    --     for all jobs when the jobRunner is "run").
    --
    function asyncJobRunner:onFinishCurrentJob( ... )
        logger:debug("Finish current job")
        if not self.currentJob.onFinishJob or self.currentJob.onFinishJob(self.currentJob.jobState, unpack(arg)) then
            -- There either wasn't a registered onFinishJob handler, or there was and it returned true, indicating success...
            --
            self:onStartNextJob()
        else
            -- There was a registered onFinishJob handler, and when called, it returned false, indicating failure...
            --
            logger:debug("currentRetryCount: %i, job.retryCount: %i", self.currentRetryCount, self.currentJob.retryCount)
            if self.currentRetryCount < self.currentJob.retryCount then
                logger:error("Job failed, retrying")
                self.currentRetryCount = self.currentRetryCount + 1
                self:onStartCurrentJob()
            else
                logger:error("Job failed, terminating job runner")
                self.currentJobIndex = nil
                if self.eventListener then
                    self.eventListener({phase = "runjob", previousJob = self.currentJob.jobState})
                    self.eventListener({phase = "failed"})
                end
            end
        end
    end
    
    function asyncJobRunner:onCancelCurrentJob( )
        logger:debug("onCancelCurrentJob")
        if self.currentJob and self.currentJob.onCancelJob then
            logger:debug("cancelling job!")
            self.currentJob.onCancelJob(self.currentJob.jobState)
        end
    end
    
    function asyncJobRunner:addEventListener( listener )
        self.eventListener = listener
    end
        
    function asyncJobRunner:addJob( jobState, onStartJob, onFinishJob, onCancelJob, retryCount )
        local asyncJob = {
            jobState = jobState,
            onStartJob = onStartJob,
            onFinishJob = onFinishJob,
            onCancelJob = onCancelJob,
            retryCount = retryCount or 0,
        }
        
        table.insert(self.jobs, asyncJob)
    end

    function asyncJobRunner:run( )
        if #self.jobs > 0 then

            if self.eventListener then
                self.eventListener({phase = "began"})
            end
        
            self:onStartNextJob()

            local cancellable = {
                cancel = function( )
                    asyncJobRunner:onCancelCurrentJob()
                    asyncJobRunner.currentJob = nil
                    if asyncJobRunner.eventListener then
                        asyncJobRunner.eventListener({phase = "cancelled"})
                    end
                end
            }
            
            return cancellable
        end
    end

    return asyncJobRunner    
end

function M.testAsyncJobRunner()

    local function onStartJob1( jobState, onComplete )
        timer.performWithDelay(1000, onComplete)
        return true
    end
    
    local function onStartJob2( jobState, onComplete )
        timer.performWithDelay(2000, onComplete)
        return true
    end

    local function onFinishJob2( jobState )
        if jobState.retry then
            logger:info("Job 2 finished successfully")
            return true
        else
            logger:info("Job 2 failed")
            jobState.retry = true
            return false
        end
    end

    local function onStartJob3( jobState, onComplete )
        timer.performWithDelay(3000, onComplete)
        return true
    end

    local function onJobEvent( event )
        -- event.lastJobState, event.nextJobState, event.retryCount
        logger:info(event, "job runner event")
        if event.phase == "began" then
             logger:info("began")
        elseif event.phase == "runjob" then
             logger:info("runjob")
        elseif event.phase == "ended" then
             logger:info("ended")
        end
    end

    local jobRunner = M.createAsyncJobRunner()
            
    jobRunner:addJob({name = "Job 1"}, onStartJob1)
    jobRunner:addJob({name = "Job 2"}, onStartJob2, onFinishJob2, nil, 1)
    jobRunner:addJob({name = "Job 3"}, onStartJob3)
    jobRunner:addEventListener(onJobEvent)
    
    jobRunner:run()

end

return M
