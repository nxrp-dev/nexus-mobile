------------------------------------------------------------------------------
--
-- This file is part of the Corona game engine.
-- For overview and more information on licensing please refer to README.md 
-- Home page: https://github.com/coronalabs/corona
-- Contact: support@coronalabs.com
--
------------------------------------------------------------------------------

local Library = require "CoronaLibrary"

-- Create library
local lib = Library:new{ name='network', publisherId='com.coronalabs' }

-- Helper functions that are called from the Lua extension
--

local function DEBUG(...)
	-- print("DEBUG (network) " .. string.format(unpack(arg)))
end

-- these are the HTTP methods we directly support and will organize the correct headers for
local supportedHTTPMethods = {
	["GET"] = true,
	["PUT"] = true,
	["POST"] = true,
	["HEAD"] = true,
	["DELETE"] = true,
	["PATCH"] = true,
--	["OPTIONS"] = true,
--	["TRACE"] = true,
}

-- track use of unsupported headers so we can warn about them just once
local unsupportedHTTPMethodsAlreadyWarned = { }

local function checkHTTPMethod(method, caller)
	if not supportedHTTPMethods[method] and not unsupportedHTTPMethodsAlreadyWarned[method] then
		print("WARNING: nonstandard "..caller.." method \""..method.."\" may require custom HTTP headers")
		unsupportedHTTPMethodsAlreadyWarned[method] = true
	end
end

-- validate parameters almost all of which are optional for historical reasons
local function checkCallingParams(callee, url, method, listener, params, filename, baseDirectory, contentType)

	if type(url) ~= "string" then
		error("network.request: 'url' parameter must be a string (got "..type(url)..")", 3)
	end

	if method and type(method) ~= "string" then
		error(callee..": 'method' parameter must be a string (got "..type(method)..")", 3)
	end

	-- under certain circumstances (Lua runtime event handling) the "listener" can be
	-- a table (see display.loadRemoteImage() in init.lua for an example)
	if listener and not (type(listener) == "function" or type(listener) == "table") then
		error(callee..": 'listener' parameter must be a function (got "..type(listener)..")", 3)
	end

	if params and type(params) ~= "table" then
		error(callee..": 'params' parameter must be a table (got "..type(params)..")", 3)
	end

	if filename and type(filename) ~= "string" then
		error(callee..": 'filename' parameter must be a string (got "..type(filename)..")", 3)
	end

	if baseDirectory and type(baseDirectory) ~= "userdata" then
		error(callee..": 'baseDirectory' parameter must be a system directory constant (got "..type(baseDirectory)..")", 3)
	end

	if contentType and type(contentType) ~= "string" then
		error(callee..": 'contentType' parameter must be a string (got "..type(contentType)..")", 3)
	end

end

DEBUG("Lua helper loaded")

-- TODO: Make this a "private" method of the network library
function _network_pathForFile(filename, baseDirectory)

	DEBUG("filename type: %s, value: %s", type(filename), tostring(filename))
	DEBUG("baseDirectory type: %s, value: %s", type(baseDirectory), tostring(baseDirectory))

	local path = system.pathForFile(filename, baseDirectory)

	DEBUG("path: %s", path or "nil")

	local isResourceFile = (( nil == baseDirectory ) or ( system.ResourceDirectory == baseDirectory ))
	DEBUG("Is resource file: %s", tostring(isResourceFile))

	return path, isResourceFile
end

-----------------------------------------------------------------------------------------------
-- Functions implemented in Lua and registered into the plugin module namespace
--

-- network.request( url, method, listener [, params] )
function lib.request( url, method, listener, params )

	checkCallingParams("network.request", url, method, listener, params, nil, nil)

	-- method is historically optional
	method = method and method:upper() or "GET"

	checkHTTPMethod(method, "network.request")

	return lib.request_native( url, method, listener, params )
end

-- network.download( url, method, listener [, params], filename [, baseDirectory] )
--
function lib.download( url, method, listener, params, filename, baseDirectory )

	if ( "string" == type( params ) ) then
		-- Optional params table omitted, shift params values right
		baseDirectory = filename
		filename = params
		params = nil
	end

	local params = params or {}
	if (params.progress) then
		params.progress = "download"
	else
		params.progress = nil
	end
	params.response = params.response or {}
	params.response.filename = filename
	
	if (baseDirectory == nil) then
		baseDirectory = system.DocumentsDirectory
	end

	if (system.ResourceDirectory == baseDirectory ) then
		print("WARNING: network.download() cannot save to system.ResourceDirectory, using system.DocumentsDirectory instead")

		baseDirectory = system.DocumentsDirectory
	end

	params.response.baseDirectory = baseDirectory

	checkCallingParams("network.download", url, method, listener, params, filename, baseDirectory)

	-- method is historically optional
	method = method and method:upper() or "GET"

	checkHTTPMethod(method, "network.download")

	return lib.request_native( url, method, listener, params )
end

-- network.upload( url, method, listener [, params], filename [, baseDirectory] [, contentType] )
--
function lib.upload( url, method, listener, params, filename, baseDirectory, contentType )

	if ( "string" == type( params ) ) then
		-- Optional params table omitted, shift param values right
		contentType = baseDirectory
		baseDirectory = filename
		filename = params
		params = nil
	end

	if ( "string" == type( baseDirectory ) ) then
		-- Optional baseDirectory omitted, shift params values right
		contentType = baseDirectory
		baseDirectory = nil
	end

	local params = params or {}
	if (params.progress) then
		params.progress = "upload"
	else
		params.progress = nil
	end
	params.body = params.body or {}
	params.body.filename = filename
	params.body.baseDirectory = baseDirectory
	if ( contentType ) then
		params.headers = params.headers or {}
		params.headers["Content-Type"] = contentType
	end

	checkCallingParams("network.upload", url, method, listener, params, filename, baseDirectory, contentType)

	-- method is historically optional
	method = method and method:upper() or "POST"

	checkHTTPMethod(method, "network.upload")

	return lib.request_native( url, method, listener, params )
end

-- network.canDetectNetworkStatusChanges
-- Default is false. Since 'nil' evaluates to false, we don't need to explicitly set it.

-- network.setStatusListener()
-- Default implementation is a no-op
function lib.setStatusListener()
	print( "WARNING: network.setStatusListener() is not supported on this platform" )
end

return lib
