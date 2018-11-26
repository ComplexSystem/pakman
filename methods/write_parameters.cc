#include <string>
#include <vector>
#include <ostream>
#include <sstream>

#include "vector_strtok.h"
#include "types.h"
#include "write_parameters.h"

void write_parameters(std::ostream& ostrm,
                      const std::vector<std::string>& parameter_names,
                      const std::vector<parameter_t>& parameters) {

    using namespace std;

    // Print header
    {
        stringstream sstrm;

        for (auto prmtr_name = parameter_names.cbegin();
             prmtr_name != parameter_names.cend(); prmtr_name++)
            sstrm << *prmtr_name << ",";

        sstrm.seekp(sstrm.tellp() - 1l);
        sstrm << endl;

        ostrm << sstrm.str();
    }

    // Print accepted parameter sets
    for (auto set = parameters.cbegin(); set != parameters.cend(); set++) {

        stringstream sstrm;

        vector<string> prmtr_elements;
        vector_strtok(*set, prmtr_elements, " \n\t");

        for (auto element = prmtr_elements.cbegin();
             element != prmtr_elements.cend(); element++) {
            sstrm << *element << ",";
        }

        sstrm.seekp(sstrm.tellp() - 1l);
        sstrm << endl;

        ostrm << sstrm.str();
    }
}
