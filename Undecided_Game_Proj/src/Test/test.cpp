#include "test.h"
#include <godot_cpp/variant/string.hpp>

using namespace godot;

void GDTest::_bind_methods(){
    ClassDB::bind_method(D_METHOD("CPP_Print", "String"), &GDTest::CPP_Print);
}

GDTest::GDTest(){
    set_process(true);
}

GDTest::~GDTest(){

}

void GDTest::CPP_Print(const String &printable_string){
    UtilityFunctions::print(printable_string); 
}

/*
void GDTest::_process(double delta){
    i += 1;    
    
    String stringIn = "Hello, World!";

    if(i == 100){
        i = 0;
        UtilityFunctions::print("Hello, World!");
    }
}
*/