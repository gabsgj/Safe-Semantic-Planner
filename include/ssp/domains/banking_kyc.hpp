#pragma once

#include "ssp/domains/domain_template.hpp"

namespace ssp::domains {

/**
 * @brief Banking Compliance & Commercial Loan Underwriting Domain
 * 
 * Models regulatory KYC checks, AML screening, automated scoring, and funds disbursement.
 * Vector Space: [ CreditScore (Norm 0-10), AMLRisk (0-1), CollateralRatio (0-2), KYCLevel (1-5), ComplianceScore (0-100) ]
 */
class BankingKycDomain : public DomainTemplate {
public:
    [[nodiscard]] std::string getName() const override {
        return "Banking KYC & Loan Underwriting";
    }

    [[nodiscard]] std::string getDescription() const override {
        return "Enterprise commercial credit underwriting pipeline enforcing OFAC/AML sanctions avoidance and capital reserve safety.";
    }

    [[nodiscard]] std::vector<std::string> getDimensionNames() const override {
        return {"CreditScore_Norm", "AMLRisk_Index", "Collateral_Ratio", "KYC_Level", "Compliance_Score"};
    }

    [[nodiscard]] core::PlanningProblem createProblem() const override {
        core::PlanningProblem prob;
        prob.domainName = "Banking KYC & Loan Underwriting";
        prob.initialState = 0; // Application Intake
        prob.goalState = 6;    // Funds Disbursed to Borrower
        prob.badStates = {3};  // OFAC Sanctioned Entity / AML Red Flag (Regulatory Violation)

        // States in 5D Vector Space: [CreditScore, AMLRisk, Collateral, KYCLevel, Compliance]
        prob.states = {
            {0, {1.0, 4.0, 0.05, 1.0, 95.0}, "Application_Submission_Intake"},
            {1, {3.0, 2.0, 0.10, 3.0, 90.0}, "Automated_Identity_Verification"},
            {2, {3.0, 6.0, 0.02, 4.0, 98.0}, "Institutional_Credit_Bureau_Check"},
            {3, {5.0, 2.0, 0.95, 1.0, 10.0}, "Sanctioned_Entity_AML_Hit_Hazard"},
            {4, {5.0, 6.0, 0.05, 5.0, 99.0}, "Senior_Risk_Underwriter_Audit"},
            {5, {7.0, 4.0, 0.01, 5.0, 100.0}, "Escrow_Lien_Perfection"},
            {6, {8.5, 4.0, 0.00, 5.0, 100.0}, "Loan_Disbursement_Complete_Goal"}
        };

        // Transitions: (id, from, to, baseCost, safety, reliability, available, name)
        prob.transitions = {
            {101, 0, 1, 10.0, 0.98, 0.99, true, "POST /kyc/automated_verify"},
            {102, 0, 2, 25.0, 1.00, 0.999, true, "POST /credit/equifax_prime"},
            {103, 1, 3, 5.0, 0.05, 0.50, true, "POST /screening/fast_pass_unverified"},
            {104, 1, 4, 35.0, 0.99, 0.995, true, "POST /underwriting/manual_escalation"},
            {105, 2, 4, 15.0, 1.00, 0.999, true, "POST /underwriting/tier1_review"},
            {106, 3, 5, 50.0, 0.10, 0.60, true, "POST /escrow/risky_bypass"},
            {107, 4, 5, 20.0, 1.00, 0.999, true, "POST /escrow/collateral_pledge"},
            {108, 5, 6, 8.0, 1.00, 1.000, true, "POST /wire/fedwire_disburse"}
        };

        return prob;
    }
};

} // namespace ssp::domains
