extends Node

## A single consumer, priority weighted server model.
## Good for chaotic threads, where long term data is pointless.
## This is lossy, as it will never process old requests that have been culled.
## Only has 2 indexes. Threads must fight over who gets the 2nd index.
## SYNCHRONOUS.

var _slots: Array[GenerationThreadWrapper] = [null, null]
var _mutex: Mutex = Mutex.new()

## General thread safe tasks. Priority must be LESS THAN to be accepted.
func AttemptTaskSubmit(GenerationRequestData: GenerationThreadWrapper):
	if(_mutex.try_lock()):
		var CurrentSlot = _slots[1]
		
		if(GenerationRequestData.Priority < CurrentSlot.Priority):
			_slots[1] = GenerationRequestData
			_mutex.unlock()
		else:
			_mutex.unlock()

## For speed-critical tasks
func PriorityTaskSubmit(GenerationRequestData: GenerationThreadWrapper):
	_mutex.lock()
	_slots[0] = GenerationRequestData as GenerationThreadWrapper
	_mutex.unlock()
	
func _process(_delta):
	_server_loop()
	
func _server_loop():
	if not _mutex.try_lock(): return
		
	var Job: GenerationThreadWrapper = null
	
	if(_slots[0] != null):
		Job = _slots[0]
		_slots[0] = null
	elif(_slots[1] != null):
		Job = _slots[1]
		_slots[1] = null
	_mutex.unlock()
	
	if Job != null:
		var result = Job.Func.call()
		if Job.CallbackFunc.is_valid(): 
			Job.CallbackFunc.call_deferred(result)
