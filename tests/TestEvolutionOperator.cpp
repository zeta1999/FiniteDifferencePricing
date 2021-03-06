/*
 * TestGrid.cpp
 *
 *  Created on: 11 Jun 2017
 *      Author: raiden
 */


#include <gtest/gtest.h>
#include <FiniteDifference/CEvolutionOperator.h>

using namespace fdpricing;



TEST (TridiagonalOperator, ExplicitEulerBase)
{
	CInputData inputData;
	inputData.S = 100.0;
	inputData.b = .002;
	inputData.sigma = .03;
	inputData.N = 21;
	inputData.T = 1.0;
	inputData.M = 10;

	CFiniteDifferenceSettings settings;
	CEvolutionOperator<ESolverType::ExplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> u(inputData, settings);

	CPayoffData payoffData;
	payoffData.payoff_i.resize(inputData.N);
	for (size_t i = 0; i < payoffData.payoff_i.size(); ++i)
		payoffData.payoff_i[i] = 5.0 + i;
	payoffData.payoff_i[inputData.N / 2] = 2.0;
	std::vector<double> xCopy(payoffData.payoff_i);

	u.Apply(payoffData);

	CGrid<EGridType::Adaptive> grid(inputData.S, 1e-3 * inputData.S, 10.0 * inputData.S, inputData.N);
	const double dt = inputData.T / inputData.M;
	for (size_t i = 1; i < inputData.N - 1; ++i)
	{
		const double dxPlus  = grid.Get(i + 1) - grid.Get(i);
		const double dxMinus = grid.Get(i)     - grid.Get(i - 1);
		const double dx = dxPlus + dxMinus;

		const double drift = inputData.b * grid.Get(i);
		const double volatility = inputData.sigma * inputData.sigma * grid.Get(i) * grid.Get(i);

		const double mMinus = dt * (-dxPlus * drift + volatility) / (dxMinus * dx);
		const double mPlus  = dt * (dxMinus * drift + volatility) / (dxPlus  * dx);
		const double mZero  = 1.0 - mPlus - mMinus;

		const double x_i = mMinus * xCopy[i - 1] + mZero * xCopy[i] + mPlus * xCopy[i + 1];
		ASSERT_NEAR(x_i, payoffData.payoff_i[i], 1e-12);
	}
}


TEST (TridiagonalOperator, ExplicitEulerVega)
{
	const double dSigma = 1e-4;

	CInputData inputData;
	inputData.S = 100.0;
	inputData.b = .002;
	inputData.sigma = .03;
	inputData.N = 129;
	inputData.T = 1.0;
	inputData.M = 100;

	CFiniteDifferenceSettings settings;
	CEvolutionOperator<ESolverType::ExplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::Vega> u(inputData, settings);

	CInputData inputDataPlus(inputData);
	inputDataPlus.sigma += dSigma;
	CEvolutionOperator<ESolverType::ExplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uPlus(inputDataPlus, settings);

	CInputData inputDataMinus(inputData);
	inputDataMinus.sigma -= dSigma;
	CEvolutionOperator<ESolverType::ExplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uMinus(inputDataMinus, settings);

	CPayoffData payoffData;
	payoffData.payoff_i.resize(inputData.N, 0.0);
	payoffData.payoff_i[64] = 1.0;
	CPayoffData payoffDataPlus(payoffData), payoffDataMinus(payoffData);

	payoffData.vega_i.resize(inputData.N, 0.0);
	u.Apply(payoffData);

	uPlus.Apply(payoffDataPlus);
	uMinus.Apply(payoffDataMinus);

	std::vector<double> vega(inputData.N);
	ASSERT_EQ(vega.size(), payoffData.payoff_i.size());

	for (size_t i = 0; i < inputData.N; ++i)
	{
		vega[i] = 1.0 / (2.0 * dSigma) * (payoffDataPlus.payoff_i[i] - payoffDataMinus.payoff_i[i]);
		ASSERT_NEAR(vega[i], payoffData.vega_i[i], 1e-6);
	}
}


TEST (TridiagonalOperator, ExplicitEulerRhoBorrow)
{
	const double db = 1e-4;

	CInputData inputData;
	inputData.S = 100.0;
	inputData.b = .002;
	inputData.sigma = .03;
	inputData.N = 129;
	inputData.T = 1.0;
	inputData.M = 100;

	CFiniteDifferenceSettings settings;
	CEvolutionOperator<ESolverType::ExplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::Rho> u(inputData, settings);

	CInputData inputDataPlus(inputData);
	inputDataPlus.b += db;
	CEvolutionOperator<ESolverType::ExplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uPlus(inputDataPlus, settings);

	CInputData inputDataMinus(inputData);
	inputDataMinus.b -= db;
	CEvolutionOperator<ESolverType::ExplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uMinus(inputDataMinus, settings);

	CPayoffData payoffData;
	payoffData.payoff_i.resize(inputData.N, 0.0);
	payoffData.payoff_i[64] = 1.0;
	CPayoffData payoffDataPlus(payoffData), payoffDataMinus(payoffData);

	payoffData.rhoBorrow_i.resize(inputData.N, 0.0);
	payoffData.rho_i.resize(inputData.N, 0.0);
	u.Apply(payoffData);

	uPlus.Apply(payoffDataPlus);
	uMinus.Apply(payoffDataMinus);

	std::vector<double> rhoBorrow(inputData.N);
	ASSERT_EQ(rhoBorrow.size(), payoffData.payoff_i.size());

	for (size_t i = 0; i < inputData.N; ++i)
	{
		rhoBorrow[i] = 1.0 / (2.0 * db) * (payoffDataPlus.payoff_i[i] - payoffDataMinus.payoff_i[i]);
		ASSERT_NEAR(rhoBorrow[i], payoffData.rhoBorrow_i[i], 1e-6);
	}
}


TEST (TridiagonalOperator, ExplicitEulerAll)
{
	const double dSigma = 1e-4;
	const double db = 1e-4;

	CInputData inputData;
	inputData.S = 100.0;
	inputData.b = .002;
	inputData.sigma = .03;
	inputData.N = 129;
	inputData.T = 1.0;
	inputData.M = 100;

	CFiniteDifferenceSettings settings;
	CEvolutionOperator<ESolverType::ExplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::All> u(inputData, settings);

	CInputData inputDataPlus(inputData);
	inputDataPlus.sigma += dSigma;
	CEvolutionOperator<ESolverType::ExplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uPlus(inputDataPlus, settings);

	CInputData inputDataMinus(inputData);
	inputDataMinus.sigma -= dSigma;
	CEvolutionOperator<ESolverType::ExplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uMinus(inputDataMinus, settings);

	CInputData inputDataPlus2(inputData);
	inputDataPlus2.b += db;
	CEvolutionOperator<ESolverType::ExplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uPlus2(inputDataPlus2, settings);

	CInputData inputDataMinus2(inputData);
	inputDataMinus2.b -= db;
	CEvolutionOperator<ESolverType::ExplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uMinus2(inputDataMinus2, settings);

	CPayoffData payoffData;
	payoffData.payoff_i.resize(inputData.N, 0.0);
	payoffData.payoff_i[64] = 1.0;
	CPayoffData payoffDataPlus(payoffData), payoffDataMinus(payoffData);
	CPayoffData payoffDataPlus2(payoffData), payoffDataMinus2(payoffData);

	payoffData.vega_i.resize(inputData.N, 0.0);
	payoffData.rhoBorrow_i.resize(inputData.N, 0.0);
	payoffData.rho_i.resize(inputData.N, 0.0);
	u.Apply(payoffData);

	uPlus.Apply(payoffDataPlus);
	uMinus.Apply(payoffDataMinus);
	uPlus2.Apply(payoffDataPlus2);
	uMinus2.Apply(payoffDataMinus2);

	std::vector<double> vega(inputData.N);
	std::vector<double> rhoBorrow(inputData.N);
	ASSERT_EQ(vega.size(), payoffData.payoff_i.size());

	for (size_t i = 0; i < inputData.N; ++i)
	{
		vega[i] = 1.0 / (2.0 * dSigma) * (payoffDataPlus.payoff_i[i] - payoffDataMinus.payoff_i[i]);
		ASSERT_NEAR(vega[i], payoffData.vega_i[i], 1e-6);

		rhoBorrow[i] = 1.0 / (2.0 * db) * (payoffDataPlus2.payoff_i[i] - payoffDataMinus2.payoff_i[i]);
	}
}


TEST (TridiagonalOperator, ImplicitEulerBase)
{
	CInputData inputData;
	inputData.S = 100.0;
	inputData.b = .002;
	inputData.sigma = .03;
	inputData.N = 21;
	inputData.T = 1.0;
	inputData.M = 10;

	CFiniteDifferenceSettings settings;
	CEvolutionOperator<ESolverType::ImplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> u(inputData, settings);

	CPayoffData payoffData;
	payoffData.payoff_i.resize(inputData.N);
	for (size_t i = 0; i < payoffData.payoff_i.size(); ++i)
		payoffData.payoff_i[i] = 5.0 + i;
	payoffData.payoff_i[inputData.N / 2] = 2.0;
	std::vector<double> x(payoffData.payoff_i);

	u.Apply(payoffData);

	CGrid<EGridType::Adaptive> grid(inputData.S, 1e-3 * inputData.S, 10.0 * inputData.S,  inputData.N);
	const double dt = inputData.T / inputData.M;
	std::vector<std::array<double, 3>> mat(inputData.N);

	double dx = grid.Get(1) - grid.Get(0);
	double volatility = inputData.sigma * inputData.sigma * grid.Get(0) * grid.Get(0);
	mat[0][1] = 1.0 + dt * volatility / (dx * dx);
	mat[0][2] = -dt * volatility / (dx * dx);

	for (size_t i = 1; i < inputData.N - 1; ++i)
	{
		const double dxPlus  = grid.Get(i + 1) - grid.Get(i);
		const double dxMinus = grid.Get(i)     - grid.Get(i - 1);
		const double dx = dxPlus + dxMinus;

		const double drift = inputData.b * grid.Get(i);
		const double volatility = inputData.sigma * inputData.sigma * grid.Get(i) * grid.Get(i);

		mat[i][0] = -dt * (-dxPlus * drift + volatility) / (dxMinus * dx);
		mat[i][2]  = -dt * (dxMinus * drift + volatility) / (dxPlus  * dx);
		mat[i][1]  = 1.0 - mat[i][0] - mat[i][2];
	}

	dx = grid.Get(inputData.N - 1) - grid.Get(inputData.N - 2);
	volatility = inputData.sigma * inputData.sigma * grid.Get(inputData.N - 1) * grid.Get(inputData.N - 1);
	mat[inputData.N - 1][1] = 1.0 + dt * volatility / (dx * dx);
	mat[inputData.N - 1][0] = -dt * volatility / (dx * dx);

	std::vector<double> solve_cache;
    if (!solve_cache.size())
    {
    	solve_cache.resize(inputData.N);
    	for (size_t i = 0; i < inputData.N; ++i)
    		solve_cache[i] = mat[i][2];
    }

    solve_cache[0] = mat[0][2] / mat[0][1];
    x[0] = x[0] / mat[0][1];

    for (size_t i = 1; i < inputData.N; ++i)
    {
        const double m = 1.0 / (mat[i][1] - mat[i][0] * solve_cache[i - 1]);
        solve_cache[i] = mat[i][2] * m;
        x[i] = (x[i] - mat[i][0] * x[i - 1]) * m;
    }

    for (size_t i = inputData.N - 1; (i--) > 0 ; )
    {
        x[i] -= solve_cache[i] * x[i + 1];
    }

	for (size_t i = 0; i < inputData.N; ++i)
	{
		ASSERT_NEAR(x[i], payoffData.payoff_i[i], 1e-12);
	}
}


TEST (TridiagonalOperator, ImplicitEulerVega)
{
	const double dSigma = 1e-4;

	CInputData inputData;
	inputData.S = 100.0;
	inputData.b = .002;
	inputData.sigma = .03;
	inputData.N = 129;
	inputData.T = 1.0;
	inputData.M = 100;

	CFiniteDifferenceSettings settings;
	CEvolutionOperator<ESolverType::ImplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::Vega> u(inputData, settings);

	CInputData inputDataPlus(inputData);
	inputDataPlus.sigma += dSigma;
	CEvolutionOperator<ESolverType::ImplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uPlus(inputDataPlus, settings);

	CInputData inputDataMinus(inputData);
	inputDataMinus.sigma -= dSigma;
	CEvolutionOperator<ESolverType::ImplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uMinus(inputDataMinus, settings);

	CPayoffData payoffData;
	payoffData.payoff_i.resize(inputData.N, 0.0);
	payoffData.payoff_i[64] = 1.0;
	CPayoffData payoffDataPlus(payoffData), payoffDataMinus(payoffData);

	payoffData.vega_i.resize(inputData.N, 0.0);
	u.Apply(payoffData);

	uPlus.Apply(payoffDataPlus);
	uMinus.Apply(payoffDataMinus);

	std::vector<double> vega(inputData.N);
	ASSERT_EQ(vega.size(), payoffData.payoff_i.size());

	for (size_t i = 0; i < inputData.N; ++i)
	{
		vega[i] = 1.0 / (2.0 * dSigma) * (payoffDataPlus.payoff_i[i] - payoffDataMinus.payoff_i[i]);
		ASSERT_NEAR(vega[i], payoffData.vega_i[i], 1e-6);
	}
}


TEST (TridiagonalOperator, ImplicitEulerRhoBorrow)
{
	const double db = 1e-4;

	CInputData inputData;
	inputData.S = 100.0;
	inputData.b = .002;
	inputData.sigma = .03;
	inputData.N = 129;
	inputData.T = 1.0;
	inputData.M = 100;

	CFiniteDifferenceSettings settings;
	CEvolutionOperator<ESolverType::ImplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::Rho> u(inputData, settings);

	CInputData inputDataPlus(inputData);
	inputDataPlus.b += db;
	CEvolutionOperator<ESolverType::ImplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uPlus(inputDataPlus, settings);

	CInputData inputDataMinus(inputData);
	inputDataMinus.b -= db;
	CEvolutionOperator<ESolverType::ImplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uMinus(inputDataMinus, settings);

	CPayoffData payoffData;
	payoffData.payoff_i.resize(inputData.N, 0.0);
	payoffData.payoff_i[64] = 1.0;
	CPayoffData payoffDataPlus(payoffData), payoffDataMinus(payoffData);

	payoffData.rhoBorrow_i.resize(inputData.N, 0.0);
	payoffData.rho_i.resize(inputData.N, 0.0);
	u.Apply(payoffData);

	uPlus.Apply(payoffDataPlus);
	uMinus.Apply(payoffDataMinus);

	std::vector<double> rhoBorrow(inputData.N);
	ASSERT_EQ(rhoBorrow.size(), payoffData.payoff_i.size());

	for (size_t i = 0; i < inputData.N; ++i)
	{
		rhoBorrow[i] = 1.0 / (2.0 * db) * (payoffDataPlus.payoff_i[i] - payoffDataMinus.payoff_i[i]);
		ASSERT_NEAR(rhoBorrow[i], payoffData.rhoBorrow_i[i], 1e-6);
	}
}


TEST (TridiagonalOperator, ImplicitEulerAll)
{
	const double dSigma = 1e-4;
	const double db = 1e-4;

	CInputData inputData;
	inputData.S = 100.0;
	inputData.b = .002;
	inputData.sigma = .03;
	inputData.N = 129;
	inputData.T = 1.0;
	inputData.M = 100;

	CFiniteDifferenceSettings settings;
	CEvolutionOperator<ESolverType::ImplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::All> u(inputData, settings);

	CInputData inputDataPlus(inputData);
	inputDataPlus.sigma += dSigma;
	CEvolutionOperator<ESolverType::ImplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uPlus(inputDataPlus, settings);

	CInputData inputDataMinus(inputData);
	inputDataMinus.sigma -= dSigma;
	CEvolutionOperator<ESolverType::ImplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uMinus(inputDataMinus, settings);

	CInputData inputDataPlus2(inputData);
	inputDataPlus2.b += db;
	CEvolutionOperator<ESolverType::ImplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uPlus2(inputDataPlus2, settings);

	CInputData inputDataMinus2(inputData);
	inputDataMinus2.b -= db;
	CEvolutionOperator<ESolverType::ImplicitEuler, EGridType::Adaptive, EAdjointDifferentiation::None> uMinus2(inputDataMinus2, settings);

	CPayoffData payoffData;
	payoffData.payoff_i.resize(inputData.N, 0.0);
	payoffData.payoff_i[64] = 1.0;
	CPayoffData payoffDataPlus(payoffData), payoffDataMinus(payoffData);
	CPayoffData payoffDataPlus2(payoffData), payoffDataMinus2(payoffData);

	payoffData.vega_i.resize(inputData.N, 0.0);
	payoffData.rhoBorrow_i.resize(inputData.N, 0.0);
	payoffData.rho_i.resize(inputData.N, 0.0);
	u.Apply(payoffData);

	uPlus.Apply(payoffDataPlus);
	uMinus.Apply(payoffDataMinus);
	uPlus2.Apply(payoffDataPlus2);
	uMinus2.Apply(payoffDataMinus2);

	std::vector<double> vega(inputData.N);
	std::vector<double> rhoBorrow(inputData.N);
	ASSERT_EQ(vega.size(), payoffData.payoff_i.size());

	for (size_t i = 0; i < inputData.N; ++i)
	{
		vega[i] = 1.0 / (2.0 * dSigma) * (payoffDataPlus.payoff_i[i] - payoffDataMinus.payoff_i[i]);
		ASSERT_NEAR(vega[i], payoffData.vega_i[i], 1e-6);

		rhoBorrow[i] = 1.0 / (2.0 * db) * (payoffDataPlus2.payoff_i[i] - payoffDataMinus2.payoff_i[i]);
	}
}


TEST (TridiagonalOperator, CrankNicolsonBase)
{
	CInputData inputData;
	inputData.S = 100.0;
	inputData.b = .002;
	inputData.sigma = .03;
	inputData.N = 21;
	inputData.T = 1.0;
	inputData.M = 10;

	CFiniteDifferenceSettings settings;
	CEvolutionOperator<ESolverType::CrankNicolson, EGridType::Adaptive, EAdjointDifferentiation::None> u(inputData, settings);

	CPayoffData payoffData;
	payoffData.payoff_i.resize(inputData.N);
	for (size_t i = 0; i < payoffData.payoff_i.size(); ++i)
		payoffData.payoff_i[i] = 5.0 + i;
	payoffData.payoff_i[inputData.N / 2] = 2.0;
	std::vector<double> x(payoffData.payoff_i);

	u.Apply(payoffData);

	CGrid<EGridType::Adaptive> grid(inputData.S, 1e-3 * inputData.S, 10.0 * inputData.S,  inputData.N);
	const double dt = inputData.T / inputData.M;
	std::vector<std::array<double, 3>> mat(inputData.N);

	double dx = grid.Get(1) - grid.Get(0);
	double volatility = inputData.sigma * inputData.sigma * grid.Get(0) * grid.Get(0);
	mat[0][1] = 1.0 + .5 * dt * volatility / (dx * dx);
	mat[0][2] = -.5 * dt * volatility / (dx * dx);

	for (size_t i = 1; i < inputData.N - 1; ++i)
	{
		const double dxPlus  = grid.Get(i + 1) - grid.Get(i);
		const double dxMinus = grid.Get(i)     - grid.Get(i - 1);
		const double dx = dxPlus + dxMinus;

		const double drift = inputData.b * grid.Get(i);
		const double volatility = inputData.sigma * inputData.sigma * grid.Get(i) * grid.Get(i);

		mat[i][0] = -.5 * dt * (-dxPlus * drift + volatility) / (dxMinus * dx);
		mat[i][2]  = -.5 * dt * (dxMinus * drift + volatility) / (dxPlus  * dx);
		mat[i][1]  = 1.0 - mat[i][0] - mat[i][2];
	}

	dx = grid.Get(inputData.N - 1) - grid.Get(inputData.N - 2);
	volatility = inputData.sigma * inputData.sigma * grid.Get(inputData.N - 1) * grid.Get(inputData.N - 1);
	mat[inputData.N - 1][1] = 1.0 + .5 * dt * volatility / (dx * dx);
	mat[inputData.N - 1][0] = -.5 * dt * volatility / (dx * dx);

	std::vector<double> solve_cache;
    if (!solve_cache.size())
    {
    	solve_cache.resize(inputData.N);
    	for (size_t i = 0; i < inputData.N; ++i)
    		solve_cache[i] = mat[i][2];
    }

    solve_cache[0] = mat[0][2] / mat[0][1];
    x[0] = x[0] / mat[0][1];

    for (size_t i = 1; i < inputData.N; ++i)
    {
        const double m = 1.0 / (mat[i][1] - mat[i][0] * solve_cache[i - 1]);
        solve_cache[i] = mat[i][2] * m;
        x[i] = (x[i] - mat[i][0] * x[i - 1]) * m;
    }

    for (size_t i = inputData.N - 1; (i--) > 0 ; )
    {
        x[i] -= solve_cache[i] * x[i + 1];
    }

	dx = grid.Get(1) - grid.Get(0);
	volatility = inputData.sigma * inputData.sigma * grid.Get(0) * grid.Get(0);
	mat[0][1] = 1.0 - .5 * dt * volatility / (dx * dx);
	mat[0][2] = .5 * dt * volatility / (dx * dx);

    for (size_t i = 1; i < inputData.N - 1; ++i)
	{
		const double dxPlus  = grid.Get(i + 1) - grid.Get(i);
		const double dxMinus = grid.Get(i)     - grid.Get(i - 1);
		const double dx = dxPlus + dxMinus;

		const double drift = inputData.b * grid.Get(i);
		const double volatility = inputData.sigma * inputData.sigma * grid.Get(i) * grid.Get(i);

		mat[i][0] = .5 * dt * (-dxPlus * drift + volatility) / (dxMinus * dx);
		mat[i][2] = .5 * dt * (dxMinus * drift + volatility) / (dxPlus  * dx);
		mat[i][1] = 1.0 - mat[i][0] - mat[i][2];
	}

	dx = grid.Get(inputData.N - 1) - grid.Get(inputData.N - 2);
	volatility = inputData.sigma * inputData.sigma * grid.Get(inputData.N - 1) * grid.Get(inputData.N - 1);
	mat[inputData.N - 1][1] = 1.0 - .5 * dt * volatility / (dx * dx);
	mat[inputData.N - 1][0] = .5 * dt * volatility / (dx * dx);

    std::vector<double> xCopy(x);
    for (size_t i = 0; i < inputData.N; ++i)
    {
		const double x_i = mat[i][0] * xCopy[i - 1] + mat[i][1] * xCopy[i] + mat[i][2] * xCopy[i + 1];
		ASSERT_NEAR(x_i, payoffData.payoff_i[i], 1e-12);
    }
}


TEST (TridiagonalOperator, CrankNicolsonVega)
{
	const double dSigma = 1e-4;

	CInputData inputData;
	inputData.S = 100.0;
	inputData.b = .002;
	inputData.sigma = .03;
	inputData.N = 129;
	inputData.T = 1.0;
	inputData.M = 100;

	CFiniteDifferenceSettings settings;
	CEvolutionOperator<ESolverType::CrankNicolson, EGridType::Adaptive, EAdjointDifferentiation::Vega> u(inputData, settings);

	CInputData inputDataPlus(inputData);
	inputDataPlus.sigma += dSigma;
	CEvolutionOperator<ESolverType::CrankNicolson, EGridType::Adaptive, EAdjointDifferentiation::None> uPlus(inputDataPlus, settings);

	CInputData inputDataMinus(inputData);
	inputDataMinus.sigma -= dSigma;
	CEvolutionOperator<ESolverType::CrankNicolson, EGridType::Adaptive, EAdjointDifferentiation::None> uMinus(inputDataMinus, settings);

	CPayoffData payoffData;
	payoffData.payoff_i.resize(inputData.N, 0.0);
	payoffData.payoff_i[64] = 1.0;
	CPayoffData payoffDataPlus(payoffData), payoffDataMinus(payoffData);

	payoffData.vega_i.resize(inputData.N, 0.0);
	u.Apply(payoffData);

	uPlus.Apply(payoffDataPlus);
	uMinus.Apply(payoffDataMinus);

	std::vector<double> vega(inputData.N);
	ASSERT_EQ(vega.size(), payoffData.payoff_i.size());

	for (size_t i = 0; i < inputData.N; ++i)
	{
		vega[i] = 1.0 / (2.0 * dSigma) * (payoffDataPlus.payoff_i[i] - payoffDataMinus.payoff_i[i]);
		ASSERT_NEAR(vega[i], payoffData.vega_i[i], 1e-6);
	}
}


TEST (TridiagonalOperator, CrankNicolsonRhoBorrow)
{
	const double db = 1e-4;

	CInputData inputData;
	inputData.S = 100.0;
	inputData.b = .002;
	inputData.sigma = .03;
	inputData.N = 129;
	inputData.T = 1.0;
	inputData.M = 100;

	CFiniteDifferenceSettings settings;
	CEvolutionOperator<ESolverType::CrankNicolson, EGridType::Adaptive, EAdjointDifferentiation::Rho> u(inputData, settings);

	CInputData inputDataPlus(inputData);
	inputDataPlus.b += db;
	CEvolutionOperator<ESolverType::CrankNicolson, EGridType::Adaptive, EAdjointDifferentiation::None> uPlus(inputDataPlus, settings);

	CInputData inputDataMinus(inputData);
	inputDataMinus.b -= db;
	CEvolutionOperator<ESolverType::CrankNicolson, EGridType::Adaptive, EAdjointDifferentiation::None> uMinus(inputDataMinus, settings);

	CPayoffData payoffData;
	payoffData.payoff_i.resize(inputData.N, 0.0);
	payoffData.payoff_i[64] = 1.0;
	CPayoffData payoffDataPlus(payoffData), payoffDataMinus(payoffData);

	payoffData.rhoBorrow_i.resize(inputData.N, 0.0);
	payoffData.rho_i.resize(inputData.N, 0.0);
	u.Apply(payoffData);

	uPlus.Apply(payoffDataPlus);
	uMinus.Apply(payoffDataMinus);

	std::vector<double> rhoBorrow(inputData.N);
	ASSERT_EQ(rhoBorrow.size(), payoffData.payoff_i.size());

	for (size_t i = 0; i < inputData.N; ++i)
	{
		rhoBorrow[i] = 1.0 / (2.0 * db) * (payoffDataPlus.payoff_i[i] - payoffDataMinus.payoff_i[i]);
		ASSERT_NEAR(rhoBorrow[i], payoffData.rhoBorrow_i[i], 1e-6);
	}
}


TEST (TridiagonalOperator, CrankNicolsonAll)
{
	const double dSigma = 1e-4;
	const double db = 1e-4;

	CInputData inputData;
	inputData.S = 100.0;
	inputData.b = .002;
	inputData.sigma = .03;
	inputData.N = 129;
	inputData.T = 1.0;
	inputData.M = 100;

	CFiniteDifferenceSettings settings;
	CEvolutionOperator<ESolverType::CrankNicolson, EGridType::Adaptive, EAdjointDifferentiation::All> u(inputData, settings);

	CInputData inputDataPlus(inputData);
	inputDataPlus.sigma += dSigma;
	CEvolutionOperator<ESolverType::CrankNicolson, EGridType::Adaptive, EAdjointDifferentiation::None> uPlus(inputDataPlus, settings);

	CInputData inputDataMinus(inputData);
	inputDataMinus.sigma -= dSigma;
	CEvolutionOperator<ESolverType::CrankNicolson, EGridType::Adaptive, EAdjointDifferentiation::None> uMinus(inputDataMinus, settings);

	CInputData inputDataPlus2(inputData);
	inputDataPlus2.b += db;
	CEvolutionOperator<ESolverType::CrankNicolson, EGridType::Adaptive, EAdjointDifferentiation::None> uPlus2(inputDataPlus2, settings);

	CInputData inputDataMinus2(inputData);
	inputDataMinus2.b -= db;
	CEvolutionOperator<ESolverType::CrankNicolson, EGridType::Adaptive, EAdjointDifferentiation::None> uMinus2(inputDataMinus2, settings);

	CPayoffData payoffData;
	payoffData.payoff_i.resize(inputData.N, 0.0);
	payoffData.payoff_i[64] = 1.0;
	CPayoffData payoffDataPlus(payoffData), payoffDataMinus(payoffData);
	CPayoffData payoffDataPlus2(payoffData), payoffDataMinus2(payoffData);

	payoffData.vega_i.resize(inputData.N, 0.0);
	payoffData.rhoBorrow_i.resize(inputData.N, 0.0);
	payoffData.rho_i.resize(inputData.N, 0.0);
	u.Apply(payoffData);

	uPlus.Apply(payoffDataPlus);
	uMinus.Apply(payoffDataMinus);
	uPlus2.Apply(payoffDataPlus2);
	uMinus2.Apply(payoffDataMinus2);

	std::vector<double> vega(inputData.N);
	std::vector<double> rhoBorrow(inputData.N);
	ASSERT_EQ(vega.size(), payoffData.payoff_i.size());

	for (size_t i = 0; i < inputData.N; ++i)
	{
		vega[i] = 1.0 / (2.0 * dSigma) * (payoffDataPlus.payoff_i[i] - payoffDataMinus.payoff_i[i]);
		ASSERT_NEAR(vega[i], payoffData.vega_i[i], 1e-6);

		rhoBorrow[i] = 1.0 / (2.0 * db) * (payoffDataPlus2.payoff_i[i] - payoffDataMinus2.payoff_i[i]);
	}
}



