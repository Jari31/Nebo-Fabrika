#include "test.h"

using namespace godot;

void GDTest::_bind_methods(){

}

GDTest::GDTest(){

}

GDTest::~GDTest(){

}

void GDTest::Print(){
    godot::String helloWorld = "Hello World!";

    godot::UtilityFunctions::print(helloWorld); 
}