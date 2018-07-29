#define sandbox_fields_reflection_testlib_class_testStruct(f, g, ...) \
	f(unsigned long, fieldLong, ##__VA_ARGS__) \
	g() \
	f(const char*, fieldString, ##__VA_ARGS__) \
	g() \
	f(unsigned int, fieldBool, ##__VA_ARGS__) \
	g() \
	f(char[8], fieldFixedArr, ##__VA_ARGS__) \
	g() \
	f(int (*)(unsigned, const char*, unsigned[1]), fieldFnPtr, ##__VA_ARGS__) \
	g()

#define sandbox_fields_reflection_testlib_allClasses(f, ...) \
	f(testStruct, testlib, ##__VA_ARGS__)
