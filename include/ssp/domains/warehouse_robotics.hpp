#pragma once

#include "ssp/domains/domain_template.hpp"

namespace ssp::domains {

/**
 * @brief Autonomous Warehouse Robotics & Logistics Mesh Domain
 * 
 * Models autonomous mobile robots (AMRs) navigating a high-density fulfillment facility
 * while avoiding high-traffic forklift collision zones and battery drain hazards.
 * Vector Space: [ X_coord (m), Y_coord (m), Elevation (m), Battery_pct, Payload_kg ]
 */
class WarehouseRoboticsDomain : public DomainTemplate {
public:
    [[nodiscard]] std::string getName() const override {
        return "Warehouse Robotics & AMR Logistics";
    }

    [[nodiscard]] std::string getDescription() const override {
        return "Continuous AMR robot trajectory planner with dynamic forklift collision avoidance, battery reserve constraints, and high-speed transit corridors.";
    }

    [[nodiscard]] std::vector<std::string> getDimensionNames() const override {
        return {"X_coord_m", "Y_coord_m", "Elevation_m", "Battery_pct", "Payload_kg"};
    }

    [[nodiscard]] core::PlanningProblem createProblem() const override {
        core::PlanningProblem prob;
        prob.domainName = "Warehouse Robotics & AMR Logistics";
        prob.initialState = 0; // Inbound Loading Dock
        prob.goalState = 6;    // Outbound Automated Shipping Bay
        prob.badStates = {3};  // High-Speed Heavy Forklift Crossing Zone (Severe Collision Risk)

        // States in 5D Vector Space: [X, Y, Elevation, Battery, Payload]
        prob.states = {
            {0, {1.0, 4.0, 0.0, 95.0, 50.0}, "Inbound_Loading_Dock_AMR01"},
            {1, {3.0, 2.0, 0.0, 88.0, 50.0}, "Aisle_A_Narrow_Corridor"},
            {2, {3.0, 6.0, 0.0, 85.0, 50.0}, "Aisle_B_Overhead_Conveyor_Deck"},
            {3, {5.0, 2.0, 0.0, 75.0, 50.0}, "Heavy_Forklift_Crossway_Hazard"},
            {4, {5.0, 6.0, 1.5, 78.0, 50.0}, "Automated_Storage_ASRS_Elevator"},
            {5, {7.0, 4.0, 0.0, 70.0, 50.0}, "Sorting_Matrix_Junction"},
            {6, {8.5, 4.0, 0.0, 65.0, 0.0}, "Outbound_Dispatch_Bay_Goal"}
        };

        // Transitions: (id, from, to, baseCost, safety, reliability, available, name)
        prob.transitions = {
            {101, 0, 1, 3.0, 0.95, 0.99, true, "Transit_Aisle_A"},
            {102, 0, 2, 4.0, 1.00, 0.999, true, "Transit_Aisle_B_Ramp"},
            {103, 1, 3, 2.5, 0.10, 0.70, true, "Direct_Crossway_Unprotected"},
            {104, 1, 4, 6.0, 0.98, 0.99, true, "Bypass_via_Elevator"},
            {105, 2, 4, 3.5, 1.00, 0.999, true, "ASRS_Elevated_Traverse"},
            {106, 3, 5, 3.0, 0.15, 0.75, true, "Crossway_Exit"},
            {107, 4, 5, 4.0, 1.00, 0.999, true, "Descent_to_Sorting_Junction"},
            {108, 5, 6, 2.0, 1.00, 1.000, true, "Final_Docking_Deliver"}
        };

        return prob;
    }
};

} // namespace ssp::domains
