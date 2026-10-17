--====================================================================--
-- Module: log
--====================================================================--
-- To Do:
--
--   - It would be nice to be able to trap error( ) and assert( ) output
--     and write it to the log.
--
------------------------------------------------------------------------
--
local M = {}

M.ALL    = 0
M.TRACE  = 100
M.DEBUG  = 200
M.INFO   = 300
M.WARN   = 400
M.ERROR  = 500
M.FATAL  = 600
M.OFF    = 700

M.defaultLevel = M.INFO
M.globalLimit  = M.ALL

local levelNames = {
    [M.ALL]   = "ALL",
    [M.TRACE] = "TRACE",
    [M.DEBUG] = "DEBUG",
    [M.INFO]  = "INFO",
    [M.WARN]  = "WARN",
    [M.ERROR] = "ERROR",
    [M.FATAL] = "FATAL",
    [M.OFF]   = "OFF",
}

function M.getLevelDescription( level )
    return levelNames[level]
end

local startTimeMs = system.getTimer()

function M.setModuleLevel( module, level )
    if type(module) == "table" then
        for _, moduleName in pairs(module) do 
            local logger = M.getModuleLogger(moduleName)
            logger:setLevel(level)
        end
    else
        local logger = M.getModuleLogger(module)
        logger:setLevel(level)
    end
end

local loggers = {
    -- [moduleName] = moduleLogger,
}

local function formatLogLine( module, level, str )
    local elapsedMs = system.getTimer() - startTimeMs
    local levelStr = M.getLevelDescription(level)
    -----------------------------------------------------------------
    --   2132 INFO  [Module]: This is the message
    --
    return string.format("%7i %-5s [%s]: %s", elapsedMs, levelStr, module, str)
end

local function getConsoleAppender( )

    local consoleAppender = { 
        level = M.INFO,
    }

    function consoleAppender:output( module, level, str )
        print(formatLogLine(module, level, str))
    end
    
    return consoleAppender
end

local consoleAppender = getConsoleAppender()

local appenders = { 
    consoleAppender,
}

function M.setConsoleAppenderLevel( level )
    consoleAppender.level = level
end

function M.addAppender( appender )
    table.insert(appenders, appender)
end

function M.addModuleAppender( module, appender)
    if type(module) == "table" then
        for _, moduleName in pairs(module) do 
            local logger = M.getModuleLogger(moduleName)
            logger:addAppender(appender)
        end
    else
        local logger = M.getModuleLogger(module)
        logger:addAppender(appender)
    end
end

function M.getFileAppender( level, filepath, directory )

    local filepath = filepath
    if directory then
        filepath = system.pathForFile(filepath, directory)
    end
    
    -- Since logging can't pull in any other modules without setting off a chain reaction
    -- of includes, we just reimplement a version of this here for our use below.
    --
    local function fileExists(filepath)
        local file, err = io.open(filepath, "r")
        if file then
            io.close(file)
            return true
        end
        return false
    end
    
    -- If file exists, we will rename it by putting a time stamp indicator
    -- at the end (right before the file extension, if any).
    --    
    if fileExists(filepath) then
        -- This whole atrocity is because the Corona Windows simulator likes to launch
        -- the app twice, usually within the same second, so the time-based uniqueifier was
        -- not sufficient.
        --
        local function getRenameFilepath(filepath, extension, attempt)
            local time_suffix = "_" .. os.time(os.date('*t'))
            local suffixes = {"a", "b", "c", "d", "e"}
            local renameFilepath = filepath .. time_suffix .. suffixes[attempt]
            if extension then
                renameFilepath = renameFilepath .. extension
            end
            if fileExists(renameFilepath) then
            else
                return renameFilepath
            end
        end

        local trimmed_filepath, extension = filepath:match("^(.*)(%.%a+)$")
        for i = 1, 5 do
            local renameFilepath = getRenameFilepath(trimmed_filepath, extension, i)
            if renameFilepath then
                assert(os.rename(filepath, renameFilepath))
                break
            end
        end
    end

    local fileAppender = {
        level = level,
        filepath = filepath,
    }
    
    function fileAppender:output( module, level, str )
        -- Open file in append mode, write to the end, close
        local file = assert(io.open(self.filepath, "a+"))
        file:write(formatLogLine(module, level, str) .. "\n")
        io.close(file)
    end
    
    return fileAppender
end

function M.getModuleLogger( moduleName )

    if loggers[moduleName] then
        return loggers[moduleName]
    end

    local moduleLogger = {
        __TYPE_LOGGER = true,
        moduleName = moduleName,
        moduleLevel = nil,
        moduleAppenders = {},
    }

    local typeSelfErrorMessage = "ERROR: moduleLogger expected.  Did you call this logging method with '.' instead of ':'?"
    
    function moduleLogger:addAppender( appender )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        table.insert(self.moduleAppenders, appender)
    end
    
    function moduleLogger:setLevel( level )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        self.moduleLevel = level
    end

    function moduleLogger:getLevel( )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        return self.moduleLevel
    end

    function moduleLogger:getEffectiveLevel( )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        local localLevel = self.moduleLevel or M.defaultLevel
        if localLevel >= M.globalLimit then
            return localLevel
        else
            return M.globalLimit
        end
    end

    function moduleLogger:isEnabledFor( level )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        return level >= self:getEffectiveLevel()
    end
    
    function moduleLogger:isTraceEnabled( )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        return self:isEnabledFor(M.TRACE)
    end

    function moduleLogger:isDebugEnabled( )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        return self:isEnabledFor(M.DEBUG)
    end

    function moduleLogger:isInfoEnabled( )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        return self:isEnabledFor(M.INFO)
    end

    function moduleLogger:isWarnEnabled( )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        return self:isEnabledFor(M.WARN)
    end

    function moduleLogger:isErrorEnabled( )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        return self:isEnabledFor(M.ERROR)
    end

    function moduleLogger:isFatalEnabled( )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        return self:isEnabledFor(M.FATAL)
    end
    
    local function output( logger, level, str )
        for _, appender in pairs(appenders) do
            if level >= appender.level then
                appender:output(logger.moduleName, level, str)
            end
        end
        for _, appender in pairs(logger.moduleAppenders) do
            if level >= appender.level then
                appender:output(logger.moduleName, level, str)
            end
        end
    end

    function moduleLogger:log( level, ... )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        if self:isEnabledFor(level) then
            if #arg >= 1 and type(arg[1]) == "table" then
                -- Table (with optional description)
                
                local function dumpTable( t, indentLevel )
                    if indentLevel and indentLevel > 10 then
                        -- Limit recursion to stop pathological cases
                        return
                    end
                    indentLevel = indentLevel or 0
                    prefix = prefix or ""
                    if t then
                        for k,v in pairs(t) do
                            local indent = ""
                            for i = 1, indentLevel do
                                indent = indent .. "    "
                            end
                            
                            output(self, level, indent .. "[" .. tostring(k) .. "] = " .. tostring(v))
                            if type(v) == "table" then
                                if v ~= t then -- Don't recurse self-reference
                                    output(self, level, indent .. "{")
                                    dumpTable(v, indentLevel + 1)
                                    output(self, level, indent .. "}")
                                end
                            end
                        end
                    end
                end

                local caption = tostring(arg[1])
                if #arg >= 2 then
                    caption = tostring(arg[2])
                end
                output(self, level, "------ BEGIN " .. caption .. " -----")
                dumpTable(arg[1])
                output(self, level, "------  END " .. caption .. "  -----")
            elseif #arg == 1 then 
                -- Simple (non-table) value
                output(self, level, tostring(arg[1]))
            elseif #arg > 1 then 
                -- Format string and params
                output(self, level, string.format(unpack(arg)))
            end
        end
    end
    
    function moduleLogger:trace( ... )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        self:log(M.TRACE, ...)
    end

    function moduleLogger:debug( ... )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        self:log(M.DEBUG, ...)
    end

    function moduleLogger:info( ... )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        self:log(M.INFO, ...)
    end

    function moduleLogger:warn( ... )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        self:log(M.WARN, ...)
    end

    function moduleLogger:error( ... )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        self:log(M.ERROR, ...)
    end

    function moduleLogger:fatal( ... )
        if not (type(self) == "table" and self.__TYPE_LOGGER) then error(typeSelfErrorMessage, 2) end
        self:log(M.FATAL, ...)
    end
    
    loggers[moduleName] = moduleLogger
    
    return moduleLogger
end

local logger = M.getModuleLogger("Log")

if (M.globalLimit < M.OFF) and (system.getInfo("platformName") == "iPhone OS") then
    -- Enable console logging via print() on iOS
    io.output():setvbuf('no')
	logger:info("iOS - Console logging enabled")
end

return M
