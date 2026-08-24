#pragma once

#include "ssp/domains/domain_template.hpp"

namespace ssp::domains {

/**
 * @brief Hospital Emergency Room & Critical Care Triage Domain
 * 
 * Models patient triage paths with clinical acuity, wait time, infection risk, and ICU overflow protection.
 * Vector Space: [ Acuity (1-5), TimeToCare (mins), InfectionRisk (0-1), StaffLoad (%), SterileLevel (1-5) ]
 */
class HospitalTriageDomain : public DomainTemplate {
public:
    [[nodiscard]] std::string getName() const override {
        return "Hospital Emergency Triage Pipeline";
    }

    [[nodiscard]] std::string getDescription() const override {
        return "Dynamic clinical workflow optimizing survival rates, minimizing ER bottlenecks, and strictly avoiding saturated ICU wards.";
    }

    [[nodiscard]] std::vector<std::string> getDimensionNames() const override {
        return {"Acuity_Level", "TimeToCare_min", "InfectionRisk", "StaffLoad_pct", "Sterility_Level"};
    }

    [[nodiscard]] core::PlanningProblem createProblem() const override {
        core::PlanningProblem prob;
        prob.domainName = "Hospital Emergency Triage Pipeline";
        prob.initialState = 0; // Ambulance Arrival
        prob.goalState = 6;    // Patient Stabilized & Discharged to Recovery
        prob.badStates = {3};  // Overcrowded ICU Overflow (100% Saturated / Outbreak Risk)

        // States in 5D Vector Space: [Acuity, TimeToCare, InfectionRisk, StaffLoad, Sterility]
        prob.states = {
            {0, {1.0, 4.0, 0.10, 30.0, 3.0}, "Ambulance_Arrival_Bay"},
            {1, {3.0, 2.0, 0.20, 50.0, 4.0}, "Rapid_Triage_Assessment"},
            {2, {3.0, 6.0, 0.05, 75.0, 5.0}, "Resuscitation_Trauma_OT1"},
            {3, {5.0, 2.0, 0.90, 100.0, 1.0}, "Overcrowded_ICU_Overflow_Hazard"},
            {4, {5.0, 6.0, 0.10, 60.0, 5.0}, "Emergency_Surgery_Suite_OT2"},
            {5, {7.0, 4.0, 0.05, 40.0, 5.0}, "Sterile_StepDown_Unit"},
            {6, {8.5, 4.0, 0.01, 20.0, 5.0}, "Patient_Stabilized_Goal"}
        };

        // Transitions: (id, from, to, baseCost, safety, reliability, available, name)
        prob.transitions = {
            {101, 0, 1, 5.0, 0.95, 0.98, true, "Standard_Triage_Intake"},
            {102, 0, 2, 2.0, 1.00, 0.99, true, "Code_Red_Direct_Trauma"},
            {103, 1, 3, 20.0, 0.10, 0.60, true, "Overflow_ICU_Admission_Risky"},
            {104, 1, 4, 15.0, 0.98, 0.99, true, "Prep_Emergency_Surgery"},
            {105, 2, 4, 6.0, 1.00, 0.99, true, "Transfer_to_Surgery_Suite"},
            {106, 3, 5, 35.0, 0.20, 0.70, true, "Transfer_from_Overflow"},
            {107, 4, 5, 12.0, 1.00, 0.99, true, "Post_Op_Sterile_Transfer"},
            {108, 5, 6, 8.0, 1.00, 0.99, true, "Final_Clinical_Discharge"}
        };

        return prob;
    }
};

} // namespace ssp::domains
