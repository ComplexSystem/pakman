#include <random>
#include <vector>
#include <string>
#include <sstream>
#include <iostream>
#include <stdexcept>
#include <cassert>

#include "core/common.h"
#include "core/Parameter.h"
#include "interface/protocols.h"
#include "interface/write_parameters.h"
#include "interface/read_input.h"

#include "smc_weight.h"

#include "ABCSMCController.h"

// Constructor
ABCSMCController::ABCSMCController(const Input &input_obj,
        std::shared_ptr<std::default_random_engine> p_generator,
        int pop_size) :
    m_epsilons(input_obj.epsilons),
    m_parameter_names(input_obj.parameter_names),
    m_smc_sampler(std::vector<double>(pop_size),
            std::vector<Parameter>(pop_size), p_generator, input_obj.perturber,
            input_obj.prior_sampler, input_obj.prior_pdf),
    m_perturbation_pdf(input_obj.perturbation_pdf),
    m_pop_size(pop_size),
    m_simulator(input_obj.simulator)
{
    m_smc_sampler.setT(m_t);
}

// Iterate function
void ABCSMCController::iterate()
{
    // This function should never be called recursively
    static bool entered = false;
    if (entered) throw;
    entered = true;

    // If m_t is equal to the number of epsilons, something went wrong because
    // the algorithm should have finished
    assert(m_t < m_epsilons.size());

    // Check if there are any new accepted parameters
    while (!m_p_master->finishedTasksEmpty())
    {
        // Get reference to front finished task
        AbstractMaster::TaskHandler& task = m_p_master->frontFinishedTask();

        // Do not accept any errors for now
        if (task.didErrorOccur())
        {
            std::runtime_error e("Task finished with error!");
            throw e;
        }

        // Check if parameter was accepted
        if (parse_simulator_output(task.getOutputString()))
        {
            // Declare raw parameter
            std::string raw_parameter;

            // Get input string
            std::stringstream input_sstrm(task.getInputString());

            // Discard epsilon
            std::getline(input_sstrm, raw_parameter);

            // Read accepted parameter
            std::getline(input_sstrm, raw_parameter);

            // Push accepted parameter
            m_prmtr_accepted_new.push_back(std::move(raw_parameter));
        }

        // Pop finished task
        m_p_master->popFinishedTask();
    }

    // Iterate over parameters whose weights have not yet been computed
    for (int i = m_weights_new.size(); i < m_prmtr_accepted_new.size(); i++)
        m_weights_new.push_back(smc_weight(
                m_perturbation_pdf,
                m_smc_sampler.getPriorPdf(),
                m_t,
                m_smc_sampler.getParameterPopulation(),
                m_smc_sampler.getWeights(),
                m_prmtr_accepted_new[i]));

    // If enough parameters have been accepted for this generation, check if we
    // are in the last generation.  If we are in the last generation, then
    // print the accepted parameters and terminate Master.  If we are not in
    // the last generation, then swap the weights and populations
    if (m_prmtr_accepted_new.size() >= m_pop_size)
    {
        // Trim any superfluous parameters
        while (m_prmtr_accepted_new.size() > m_pop_size)
        {
            m_prmtr_accepted_new.pop_back();
            m_weights_new.pop_back();
        }

        // Increment generation counter
        m_t++;
        m_smc_sampler.setT(m_t);

        // Check if we are in the last generation
        if (m_t == m_epsilons.size())
        {
            // Print accepted parameters
            write_parameters(std::cout, m_parameter_names, m_prmtr_accepted_new);

            // Terminate Master
            m_p_master->terminate();
            entered = false;
            return;
        }

        // Swap population and weights
        m_smc_sampler.swap_population(m_weights_new, m_prmtr_accepted_new);

        // Clear m_weights_new and m_prmtr_accepted_new
        m_weights_new.clear();
        m_prmtr_accepted_new.clear();

        // Flush Master
        m_p_master->flush();
        entered = false;
        return;
    }

    // There is still work to be done, so make sure there are as many tasks
    // queued as there are Managers
    while (m_p_master->needMorePendingTasks())
        m_p_master->pushPendingTask(format_simulator_input(m_epsilons[m_t],
                    m_smc_sampler.sampleParameter()));

    entered = false;
}

Command ABCSMCController::getSimulator() const
{
    return m_simulator;
}

// Construct from input stream
ABCSMCController::Input::Input(std::istream& istrm)
{
    // Read lines
    std::vector<std::string> lines;
    read_lines(istrm, num_lines, lines);

    // Parse epsilons
    parse_csv_list(lines[0], epsilons);

    // Get simulator
    simulator = lines[1];

    // Parse parameter names
    parse_csv_list(lines[2], parameter_names);

    // Get rest
    prior_sampler = lines[3];
    perturber = lines[4];
    prior_pdf = lines[5];
    perturbation_pdf = lines[6];
}
