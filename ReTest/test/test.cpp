#include "generate/drivergenerator.h"
#include <static/testcase.h>

int main(){
    PCTRT::TestCase tc1;
    tc1.inputs.push_back({"x", "int", "0"});
    tc1.outputs.push_back({"retVal", "int", "0"});
    
    PCTRT::TestCase tc2;
    tc2.inputs.push_back({"x", "int", "1"});
    tc2.outputs.push_back({"retVal", "int", "1"});

    PCTRT::TestSuite ts;
    ts.setExecuted(0);
    ts.setCoverage(0.0);
    ts.setSrcFile("test/reverse_old.c");
    ts.setFuncName("reverse");
    ts.setDescription("Test reverse function");
    ts.addTestCase(tc1);
    ts.addTestCase(tc2);

    nlohmann::json j = ts;
    std::cout << j.dump(4) << std::endl;
    return 0;
}