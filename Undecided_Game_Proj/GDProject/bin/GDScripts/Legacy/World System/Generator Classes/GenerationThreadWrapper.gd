class_name GenerationThreadWrapper
## A class that handles generation pooling by acting as a wrapper for the pooling system.

var Func: Callable
var CallbackFunc: Callable
var Priority: int
var Params

func Init(p_Func: Callable, p_CallbackFunc: Callable, p_Priority: int, p_Params = null):
	CallbackFunc = p_CallbackFunc
	Func = p_Func
	Priority = p_Priority
	Params = p_Params
