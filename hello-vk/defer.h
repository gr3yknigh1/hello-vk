#pragma once
///
/// @ref https://www.gingerbill.org/article/2015/08/19/defer-in-cpp/
/// 

template <typename FnType>
struct Defer_Fn_Holder {
	FnType fn;
	
	Defer_Fn_Holder(FnType fn) : fn(fn) 
	{
	}
	
	~Defer_Fn_Holder(void)
	{ 
		fn(); 
	}
};

template <typename FnType>
Defer_Fn_Holder<FnType> defer_fn(FnType fn)
{
	return Defer_Fn_Holder<FnType>(fn);
}

#define DEFER_1(x, y) x##y
#define DEFER_2(x, y) DEFER_1(x, y)
#define DEFER_3(x)    DEFER_2(x, __COUNTER__)
#define defer(code)   auto DEFER_3(_defer_) = defer_fn([&](){code;})
