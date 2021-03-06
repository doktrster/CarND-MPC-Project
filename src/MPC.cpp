#include "MPC.h"
#include <cppad/cppad.hpp>
#include <cppad/ipopt/solve.hpp>
#include "Eigen-3.3/Eigen/Core"

using CppAD::AD;
using namespace Eigen;
using namespace std;

// TODO: Set the timestep length and duration
size_t N = 7;
double dt = 0.1;

// This value assumes the model presented in the classroom is used.
//
// It was obtained by measuring the radius formed by running the vehicle in the
// simulator around in a circle with a constant steering angle and velocity on a
// flat terrain.
//
// Lf was tuned until the the radius formed by the simulating the model
// presented in the classroom matched the previous radius.
//
// This is the length from front to CoG that has a similar radius.
const double Lf = 2.67;

// NOTE: feel free to play around with this
// or do something completely different
double ref_cte = 0;
double ref_psi_error = 0;
double ref_v = 60;

// The solver takes all the state variables and actuator
// variables in a singular vector. Thus, we should to establish
// when one variable starts and another ends to make our lifes easier.
int x_start = 0;
int y_start = x_start + N;
int psi_start = y_start + N;
int v_start = psi_start + N;
int cte_start = v_start + N;
int psi_error_start = cte_start + N;
int delta_start = psi_error_start + N;
int a_start = delta_start + N - 1;


class FG_eval {
 public:
  // Fitted polynomial coefficients
  VectorXd coeffs;
  FG_eval(VectorXd coeffs)
  {
    this->coeffs = coeffs; 
  }

  typedef CPPAD_TESTVECTOR(AD<double>) ADvector;
  void operator()(ADvector& fg, const ADvector& vars)
  {
    // TODO: implement MPC
    // `fg` a vector of the cost constraints, 
    //`vars` is a vector of variable values (state & actuators)

    fg[0] = 0;

    // Reference state Cost
    // TODO: Define the cost related the reference state and
    // any anything you think may be beneficial.
    for(size_t i = 0; i < N; i++) 
    {
      fg[0] += 5000 * CppAD::pow(vars[cte_start + i] - ref_cte, 2);
      fg[0] += 5000 * CppAD::pow(vars[psi_error_start + i] - ref_psi_error, 2);
      fg[0] += CppAD::pow(vars[v_start + i] - ref_v, 2);
    }

    for(size_t i = 0; i < N - 1; i++) 
    {
      fg[0] += 10 * CppAD::pow(vars[delta_start + i], 2);
      fg[0] += 10 * CppAD::pow(vars[a_start + i], 2);
      fg[0] += 750 * (1.5 * CppAD::pow(vars[delta_start + i] * (vars[a_start + i]), 2) \
                            + CppAD::pow(vars[delta_start + i] * vars[v_start + i], 2));
    }

    for(size_t i = 0; i < N - 2; i++) 
    {
      fg[0] += 200 * CppAD::pow(vars[delta_start + i + 1] - vars[delta_start + i], 2);
      fg[0] += 10 * CppAD::pow(vars[a_start + i + 1] - vars[a_start + i], 2);
    }


    fg[1 + x_start] = vars[x_start];
    fg[1 + y_start] = vars[y_start];
    fg[1 + psi_start] = vars[psi_start];
    fg[1 + v_start] = vars[v_start];
    fg[1 + cte_start] = vars[cte_start];
    fg[1 + psi_error_start] = vars[psi_error_start];

    // The rest of the constraints
    for(size_t i = 0; i < N - 1; i++) 
    {
      AD<double> x1 = vars[x_start + i + 1];
      AD<double> y1 = vars[y_start + i + 1];
      AD<double> psi1 = vars[psi_start + i + 1];
      AD<double> v1 = vars[v_start + i + 1];
      AD<double> cte1 = vars[cte_start + i + 1];
      AD<double> psi_error1 = vars[psi_error_start + i + 1];

      AD<double> x0 = vars[x_start + i];
      AD<double> y0 = vars[y_start + i];
      AD<double> psi0 = vars[psi_start + i];
      AD<double> v0 = vars[v_start + i];
      AD<double> cte0 = vars[cte_start + i];
      AD<double> psi_error0 = vars[psi_error_start + i];
      AD<double> a0 = vars[a_start + i];
      AD<double> delta0 = vars[delta_start + i];

      AD<double> f0 = coeffs[0] + coeffs[1] * x0 + coeffs[2] * CppAD::pow(x0, 2) \
                                                         + coeffs[3] * CppAD::pow(x0, 3);

      AD<double> psides0 = CppAD::atan(3 * coeffs[3] * CppAD::pow(x0, 2) \
                                                  + 2 * coeffs[2] * x0 + coeffs[1]);

      // Here's `x` to get you started.
      // The idea here is to constraint this value to be 0.
      //
      // TODO: Setup the rest of the model constraints

      a0 = i > 0 ? vars[a_start + i - 1] : a0;
      delta0 = i > 0 ? vars[delta_start + i - 1] : delta0;

      fg[2 + x_start + i] = x1 - (x0 + v0 * CppAD::cos(psi0) * dt);
      fg[2 + y_start + i] = y1 - (y0 + v0 * CppAD::sin(psi0) * dt);
      fg[2 + psi_start + i] = psi1 - (psi0 - v0/Lf * delta0 * dt);
      fg[2 + v_start + i] = v1 - (v0 + a0 * dt);
      fg[2 + cte_start + i] = cte1 - ((f0 - y0) + (v0 * CppAD::sin(psi_error0) * dt));
      fg[2 + psi_error_start + i] = psi_error1 - ((psi0 - psides0) - v0/Lf * delta0 * dt);
    }

  }
};

//
// MPC class definition implementation.
//
MPC::MPC() {}
MPC::~MPC() {}

vector<double> MPC::Solve(VectorXd state, VectorXd coeffs)
{
  bool ok = true;
  typedef CPPAD_TESTVECTOR(double) Dvector;

  double x = state[0];
  double y = state[1];
  double psi = state[2];
  double v = state[3];
  double cte = state[4];
  double psi_error = state[5];

  // TODO: Set the number of model variables (includes both states and inputs).
  // For example: If the state is a 4 element vector, the actuators is a 2
  // element vector and there are 10 timesteps. The number of variables is:
  //
  // 4 * 10 + 2 * 9
  int n_vars = N * 6 + (N - 1) * 2;
  // TODO: Set the number of constraints
  int n_constraints = N * 6;

  // Initial value of the independent variables.
  // SHOULD BE 0 besides initial state.
  Dvector vars(n_vars);
  for(int i = 0; i < n_vars; i++) 
  {
    vars[i] = 0;
  }

  Dvector vars_lowerbound(n_vars);
  Dvector vars_upperbound(n_vars);
  // TODO: Set lower and upper limits for variables.

  // Set the initial variable values
  vars[x_start] = x;
  vars[y_start] = y;
  vars[psi_start] = psi;
  vars[v_start] = v;
  vars[cte_start] = cte;
  vars[psi_error_start] = psi_error;

  // Set all non-actuators upper and lowerlimits
  // to the max negative and positive values.
  for(int i = 0; i < delta_start; i++) 
  {
    vars_lowerbound[i] = -1.0e19;
    vars_upperbound[i] = 1.0e19;
  }

  // The upper and lower limits of delta are set to -25 and 25
  // degrees (values in radians).
  // NOTE: Feel free to change this to something else.
  for(int i = delta_start; i < a_start; i++) 
  {
    vars_lowerbound[i] = -0.436332 * Lf;
    vars_upperbound[i] = 0.436332 * Lf;
  }

  // Acceleration/decceleration upper and lower limits.
  // NOTE: Feel free to change this to something else.
  for(int i = a_start; i < n_vars; i++)
  {
    vars_lowerbound[i] = -1.0;
    vars_upperbound[i] = 1.0;
  }

  // Lower and upper limits for the constraints
  // Should be 0 besides initial state.
  Dvector constraints_lowerbound(n_constraints);
  Dvector constraints_upperbound(n_constraints);
  for(int i = 0; i < n_constraints; i++)
  {
    constraints_lowerbound[i] = 0;
    constraints_upperbound[i] = 0;
  }
  constraints_lowerbound[x_start] = x;
  constraints_lowerbound[y_start] = y;
  constraints_lowerbound[psi_start] = psi;
  constraints_lowerbound[v_start] = v;
  constraints_lowerbound[cte_start] = cte;
  constraints_lowerbound[psi_error_start] = psi_error;

  constraints_upperbound[x_start] = x;
  constraints_upperbound[y_start] = y;
  constraints_upperbound[psi_start] = psi;
  constraints_upperbound[v_start] = v;
  constraints_upperbound[cte_start] = cte;
  constraints_upperbound[psi_error_start] = psi_error;

  // object that computes objective and constraints
  FG_eval fg_eval(coeffs);

  //
  // NOTE: You don't have to worry about these options
  //
  // options for IPOPT solver
  string options;
  // Uncomment this if you'd like more print information
  options += "Integer print_level  0\n";
  // NOTE: Setting sparse to true allows the solver to take advantage
  // of sparse routines, this makes the computation MUCH FASTER. If you
  // can uncomment 1 of these and see if it makes a difference or not but
  // if you uncomment both the computation time should go up in orders of
  // magnitude.
  options += "Sparse  true        forward\n";
  options += "Sparse  true        reverse\n";
  // NOTE: Currently the solver has a maximum time limit of 0.5 seconds.
  // Change this as you see fit.
  options += "Numeric max_cpu_time          0.5\n";

  // place to return solution
  CppAD::ipopt::solve_result<Dvector> solution;

  // solve the problem
  CppAD::ipopt::solve<Dvector, FG_eval>(
      options, vars, vars_lowerbound, vars_upperbound, constraints_lowerbound,
      constraints_upperbound, fg_eval, solution);

  // Check some of the solution values
  ok &= solution.status == CppAD::ipopt::solve_result<Dvector>::success;

  // Cost
  auto cost = solution.obj_value;
  cout << "Cost " << cost << endl;   //!!!

  // TODO: Return the first actuator values. The variables can be accessed with
  // `solution.x[i]`.
  //
  // {...} is shorthand for creating a vector, so auto x1 = {1.0,2.0}
  // creates a 2 element double vector.
  vector<double> result;

  result.push_back(solution.x[delta_start]);
  result.push_back(solution.x[a_start]);

  for(size_t i = 0; i < N-1; i++) 
  {
    result.push_back(solution.x[x_start + i + 1]);
    result.push_back(solution.x[y_start + i + 1]);
  }

  return result;
}