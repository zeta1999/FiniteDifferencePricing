/*
 * CFDPricer.tpp
 *
 *  Created on: 13 Jun 2017
 *      Author: raiden
 */

#include <BlackScholes/CBlackScholes.h>
#include <Flags.h>

namespace fdpricing
{

template <ESolverType solverType, EAdjointDifferentiation adjointDifferentiation>
CFDPricer<solverType, adjointDifferentiation>::CFDPricer(const CInputData& unaliased input,
															const CPricerSettings& unaliased settings) noexcept
		: input(input), settings(settings),
		  calculateCall(settings.calculationType == ECalculationType::All || settings.calculationType == ECalculationType::CallOnly),
		  calculatePut(settings.calculationType == ECalculationType::All || settings.calculationType == ECalculationType::PutOnly),
		  accelerateCall(input.acceleration && input.b - input.r > 0.0),
		  acceleratePut(!accelerateCall),
		  u(input, settings.fdSettings)
{
	cache.discountFactor = exp(-input.r * u.GetDt());

	//const size_t nLastDiv = input.dividends.size() ? input.dividends[input.dividends.size() - 1].time / u.GetDt() : (input.N - 1);
	if (calculateCall)
	{
		callData.Init<adjointDifferentiation>(input.N);
		//nBackwardInductionCall = accelerateCall ? nLastDiv : input.N;
	}
	if (calculatePut)
	{
		putData.Init<adjointDifferentiation>(input.N);
		//nBackwardInductionPut = acceleratePut ? nLastDiv : input.N;
	}

	switch (settings.calculationType)
	{
		case ECalculationType::All:
			exerciseDelegate = &CFDPricer<solverType, adjointDifferentiation>::Exercise<ECalculationType::All>;
			smoothingDelegate = &CFDPricer<solverType, adjointDifferentiation>::PayoffSmoothing<ECalculationType::All>;
			discountDelegate = &CFDPricer<solverType, adjointDifferentiation>::RollBack<ECalculationType::All>;
			break;
		case ECalculationType::CallOnly:
			exerciseDelegate = &CFDPricer<solverType, adjointDifferentiation>::Exercise<ECalculationType::CallOnly>;
			smoothingDelegate = &CFDPricer<solverType, adjointDifferentiation>::PayoffSmoothing<ECalculationType::CallOnly>;
			discountDelegate = &CFDPricer<solverType, adjointDifferentiation>::RollBack<ECalculationType::CallOnly>;
			break;
		case ECalculationType::PutOnly:
			exerciseDelegate = &CFDPricer<solverType, adjointDifferentiation>::Exercise<ECalculationType::PutOnly>;
			smoothingDelegate = &CFDPricer<solverType, adjointDifferentiation>::PayoffSmoothing<ECalculationType::PutOnly>;
			discountDelegate = &CFDPricer<solverType, adjointDifferentiation>::RollBack<ECalculationType::PutOnly>;
			break;
		default:
			printf("WRONG SETTINGS");
			return;
	}
}

template <ESolverType solverType, EAdjointDifferentiation adjointDifferentiation>
template<ECalculationType calculationType>
void CFDPricer<solverType, adjointDifferentiation>::Exercise()
{
	const auto& grid = u.GetGrid();

	for (size_t i = 0; i < input.N; ++i)
	{
		const double intrinsicValue = grid.Get(i) - input.K;
		switch (calculationType)
		{
			case ECalculationType::All:
				if (intrinsicValue > callData.payoff_i[i])
				{
					callData.payoff_i[i] = intrinsicValue;
					callData.ZeroGreeks<adjointDifferentiation>(i);
				}
				if (-intrinsicValue > putData.payoff_i[i])
				{
					putData.payoff_i[i] = -intrinsicValue;
					putData.ZeroGreeks<adjointDifferentiation>(i);
				}
				break;
			case ECalculationType::CallOnly:
				if (intrinsicValue > callData.payoff_i[i])
				{
					callData.payoff_i[i] = intrinsicValue;
					callData.ZeroGreeks<adjointDifferentiation>(i);
				}
				break;
			case ECalculationType::PutOnly:
				if (-intrinsicValue > putData.payoff_i[i])
				{
					putData.payoff_i[i] = -intrinsicValue;
					putData.ZeroGreeks<adjointDifferentiation>(i);
				}
				break;
			default:
				break;
		}
	}
}

template <ESolverType solverType, EAdjointDifferentiation adjointDifferentiation>
template<ECalculationType calculationType>
void CFDPricer<solverType, adjointDifferentiation>::PayoffSmoothing()
{
	const auto& grid = u.GetGrid();

	cache.T = u.GetDt();
	cache.sqrtDt = sqrt(cache.T);
	cache.sigmaSqrtDt = input.sigma * cache.sqrtDt;
	cache.growthFactor   = exp( input.b * u.GetDt());
	cache.growthFactorTimesDiscountFactor = cache.discountFactor * cache.growthFactor;
	CBlackScholes bs(input, cache);

	for (size_t i = 0; i < input.N; ++i)
	{
		bs.Update(grid.Get(i));

		switch (calculationType)
		{
			case ECalculationType::All:
				#if(1) // ******* All *******

				callData.payoff_i[i] = bs.Value<EOptionType::Call>();
				putData.payoff_i[i] = bs.Value<EOptionType::Put>();

				switch (adjointDifferentiation)
				{
					case EAdjointDifferentiation::All:
						callData.vega_i[i] = putData.vega_i[i] = bs.Vega();

						callData.rho_i[i] = - u.GetDt() * callData.payoff_i[i];
						putData.rho_i[i] = - u.GetDt() * putData.payoff_i[i];
						callData.rhoBorrow_i[i] = bs.RhoBorrow<EOptionType::Call>();
						putData.rhoBorrow_i[i] = bs.RhoBorrow<EOptionType::Put>();
						break;
					case EAdjointDifferentiation::Vega:
						callData.vega_i[i] = putData.vega_i[i] = bs.Vega();
						break;
					case EAdjointDifferentiation::Rho:
						callData.rho_i[i] = - u.GetDt() * callData.payoff_i[i];
						putData.rho_i[i] = - u.GetDt() * putData.payoff_i[i];
						callData.rhoBorrow_i[i] = bs.RhoBorrow<EOptionType::Call>();
						putData.rhoBorrow_i[i] = bs.RhoBorrow<EOptionType::Put>();
						break;
					default:
						break;
				}

				#endif

				break;

			case ECalculationType::CallOnly:
				#if(1) // ****** Call *******

				callData.payoff_i[i] = bs.Value<EOptionType::Call>();

				switch (adjointDifferentiation)
				{
					case EAdjointDifferentiation::All:
						callData.vega_i[i] = bs.Vega();

						callData.rho_i[i] = - u.GetDt() * callData.payoff_i[i];
						callData.rhoBorrow_i[i] = bs.RhoBorrow<EOptionType::Call>();
						break;
					case EAdjointDifferentiation::Vega:
						callData.vega_i[i] = bs.Vega();
						break;
					case EAdjointDifferentiation::Rho:
						callData.rho_i[i] = - u.GetDt() * callData.payoff_i[i];
						callData.rhoBorrow_i[i] = bs.RhoBorrow<EOptionType::Call>();
						break;
					default:
						break;
				}

				#endif

				break;
			case ECalculationType::PutOnly:
				#if(1) // ******* Put *******

				putData.payoff_i[i] = bs.Value<EOptionType::Put>();

				switch (adjointDifferentiation)
				{
					case EAdjointDifferentiation::All:
						putData.vega_i[i] = bs.Vega();

						putData.rho_i[i] = - u.GetDt() * putData.payoff_i[i];
						putData.rhoBorrow_i[i] = bs.RhoBorrow<EOptionType::Put>();
						break;
					case EAdjointDifferentiation::Vega:
						putData.vega_i[i] = bs.Vega();
						break;
					case EAdjointDifferentiation::Rho:
						putData.rho_i[i] = - u.GetDt() * putData.payoff_i[i];
						putData.rhoBorrow_i[i] = bs.RhoBorrow<EOptionType::Put>();
						break;
					default:
						break;
				}

				#endif

				break;
			default:
				break;
		}
	}
}

template <ESolverType solverType, EAdjointDifferentiation adjointDifferentiation>
template<ECalculationType calculationType>
void CFDPricer<solverType, adjointDifferentiation>::RollBack()
{
	for (size_t i = 0; i < input.N; ++i)
	{
		switch (calculationType)
		{
			case ECalculationType::All:
				callData.payoff_i[i] *= cache.discountFactor;
				putData.payoff_i[i] *= cache.discountFactor;

				switch (adjointDifferentiation)
				{
					case EAdjointDifferentiation::All:
						callData.vega_i[i] *= cache.discountFactor;
						callData.rhoBorrow_i[i] *= cache.discountFactor;
						callData.rho_i[i] = -u.GetDt() * callData.payoff_i[i] + callData.rho_i[i] * cache.discountFactor;

						putData.vega_i[i] *= cache.discountFactor;
						putData.rhoBorrow_i[i] *= cache.discountFactor;
						putData.rho_i[i] = -u.GetDt() * putData.payoff_i[i] + putData.rho_i[i] * cache.discountFactor;
						break;
					case EAdjointDifferentiation::Vega:
						callData.vega_i[i] *= cache.discountFactor;
						putData.vega_i[i] *= cache.discountFactor;
						break;
					case EAdjointDifferentiation::Rho:
						callData.rhoBorrow_i[i] *= cache.discountFactor;
						callData.rho_i[i] = -u.GetDt() * callData.payoff_i[i] + callData.rho_i[i] * cache.discountFactor;
						putData.rhoBorrow_i[i] *= cache.discountFactor;
						putData.rho_i[i] = -u.GetDt() * putData.payoff_i[i] + putData.rho_i[i] * cache.discountFactor;
						break;
					default:
						break;
				}

				break;

			case ECalculationType::CallOnly:
				callData.payoff_i[i] *= cache.discountFactor;

				switch (adjointDifferentiation)
				{
					case EAdjointDifferentiation::All:
						callData.vega_i[i] *= cache.discountFactor;
						callData.rhoBorrow_i[i] *= cache.discountFactor;
						callData.rho_i[i] = -u.GetDt() * callData.payoff_i[i] + callData.rho_i[i] * cache.discountFactor;
						break;
					case EAdjointDifferentiation::Vega:
						callData.vega_i[i] *= cache.discountFactor;
						break;
					case EAdjointDifferentiation::Rho:
						callData.rhoBorrow_i[i] *= cache.discountFactor;
						callData.rho_i[i] = -u.GetDt() * callData.payoff_i[i] + callData.rho_i[i] * cache.discountFactor;
						break;
					default:
						break;
				}

				break;
			case ECalculationType::PutOnly:
				putData.payoff_i[i] *= cache.discountFactor;

				switch (adjointDifferentiation)
				{
					case EAdjointDifferentiation::All:
						putData.vega_i[i] *= cache.discountFactor;
						putData.rhoBorrow_i[i] *= cache.discountFactor;
						putData.rho_i[i] = -u.GetDt() * putData.payoff_i[i] + putData.rho_i[i] * cache.discountFactor;
						break;
					case EAdjointDifferentiation::Vega:
						putData.vega_i[i] *= cache.discountFactor;
						break;
					case EAdjointDifferentiation::Rho:
						putData.rhoBorrow_i[i] *= cache.discountFactor;
						putData.rho_i[i] = -u.GetDt() * putData.payoff_i[i] + putData.rho_i[i] * cache.discountFactor;
						break;
					default:
						break;
				}

				break;
			default:
				break;
		}
	}
}


template <ESolverType solverType, EAdjointDifferentiation adjointDifferentiation>
void CFDPricer<solverType, adjointDifferentiation>::BackwardInduction() noexcept
{
	if (calculateCall)
		u.Apply(callData);
	if (calculatePut)
		u.Apply(putData);

	(this->*discountDelegate)();

	if (settings.exerciseType == EExerciseType::American)
		(this->*exerciseDelegate)();
}

template <ESolverType solverType, EAdjointDifferentiation adjointDifferentiation>
void CFDPricer<solverType, adjointDifferentiation>::Price(COutputData& unaliased callOutput, COutputData& unaliased putOutput) noexcept
{
	// TODO: acceleration!
	size_t m = input.M;
	if (input.smoothing)
	{
		(this->*smoothingDelegate)();
		--m;
	}
	else
		(this->*exerciseDelegate)();

	std::array<double, 9> callLeavesDt, putLeavesDt;
	for (; m --> 0 ;)
	{
		BackwardInduction();

		if (m < 3)
			SaveLeaves(m, callLeavesDt, putLeavesDt);
	}

	ComputeGreeks(callOutput, putOutput, callLeavesDt, putLeavesDt);
	SetOutput(callOutput, putOutput);
}

template <ESolverType solverType, EAdjointDifferentiation adjointDifferentiation>
void CFDPricer<solverType, adjointDifferentiation>::SaveLeaves(const size_t m, std::array<double, 9>& unaliased callLeavesDt, std::array<double, 9>& unaliased putLeavesDt) const noexcept
{
	const size_t idx = 3 * m;
	if (calculateCall)
	{
		callLeavesDt[idx]     = callData.payoff_i[(input.N >> 1) - 1];
		callLeavesDt[idx + 1] = callData.payoff_i[input.N >> 1];
		callLeavesDt[idx + 2] = callData.payoff_i[(input.N >> 1) + 1];
	}

	if (calculatePut)
	{
		putLeavesDt[idx]     = putData.payoff_i[(input.N >> 1) - 1];
		putLeavesDt[idx + 1] = putData.payoff_i[input.N >> 1];
		putLeavesDt[idx + 2] = putData.payoff_i[(input.N >> 1) + 1];
	}
}

template <ESolverType solverType, EAdjointDifferentiation adjointDifferentiation>
void CFDPricer<solverType, adjointDifferentiation>::SetOutput(COutputData& unaliased callOutput, COutputData& unaliased putOutput) const noexcept
{
	if (calculateCall)
	{
		callOutput.price = callData.payoff_i[input.N >> 1];

		switch (adjointDifferentiation)
		{
			case EAdjointDifferentiation::All:
				callOutput.vega = callData.vega_i[input.N >> 1];
				callOutput.rhoBorrow = callData.rhoBorrow_i[input.N >> 1];
				callOutput.rho = callData.rho_i[input.N >> 1];
				break;
			case EAdjointDifferentiation::Vega:
				callOutput.vega = callData.vega_i[input.N >> 1];
				break;
			case EAdjointDifferentiation::Rho:
				callOutput.rhoBorrow = callData.rhoBorrow_i[input.N >> 1];
				callOutput.rho = callData.rho_i[input.N >> 1];
				break;
			default:
				break;
		}

	}

	if (calculatePut)
	{
		putOutput.price = putData.payoff_i[input.N >> 1];

		switch (adjointDifferentiation)
		{
			case EAdjointDifferentiation::All:
				putOutput.vega = putData.vega_i[input.N >> 1];
				putOutput.rhoBorrow = putData.rhoBorrow_i[input.N >> 1];
				putOutput.rho = putData.rho_i[input.N >> 1];
				break;
			case EAdjointDifferentiation::Vega:
				putOutput.vega = putData.vega_i[input.N >> 1];
				break;
			case EAdjointDifferentiation::Rho:
				putOutput.rhoBorrow = putData.rhoBorrow_i[input.N >> 1];
				putOutput.rho = putData.rho_i[input.N >> 1];
				break;
			default:
				break;
		}
	}
}

template <ESolverType solverType, EAdjointDifferentiation adjointDifferentiation>
void CFDPricer<solverType, adjointDifferentiation>::ComputeGreeks(COutputData& unaliased callOutput, COutputData& unaliased putOutput, const std::array<double, 9>& unaliased callLeavesDt, const std::array<double, 9>& unaliased putLeavesDt) const noexcept
{
	const auto& grid = u.GetGrid();

	const double dxPlus  = grid.Get((input.N >> 1) + 1) - grid.Get((input.N >> 1));
	const double dxMinus = grid.Get((input.N >> 1))     - grid.Get((input.N >> 1) - 1);
	const double dx = dxPlus + dxMinus;

	const double b0 = 1.0 / (dx * dxMinus);
	const double b2 = 1.0 / (dx * dxPlus);
	const double b1 = -b0 - b2;

	const double a0 = -dxPlus * b0;
	const double a2 =  dxMinus * b2;
	const double a1 = -a0 - a2;

	double oneOverHalfDt = 1.0 / u.GetDt();
	const double oneOverDt2 = oneOverHalfDt * oneOverHalfDt;
	oneOverHalfDt = .5;

	if (calculateCall)
	{
		callOutput.delta = a0 * callData.payoff_i[(input.N >> 1) - 1] + a1 * callData.payoff_i[input.N >> 1] + a2 * callData.payoff_i[(input.N >> 1) + 1];
		callOutput.gamma = 2.0 * (b0 * callData.payoff_i[(input.N >> 1) - 1] + b1 * callData.payoff_i[input.N >> 1] + b2 * callData.payoff_i[(input.N >> 1) + 1]);

		callOutput.theta  = oneOverHalfDt  * (callLeavesDt[7]                         - callLeavesDt[1]);
		callOutput.theta2 = oneOverDt2 *     (callLeavesDt[7] - 2.0 * callLeavesDt[4] + callLeavesDt[1]);

		const double delta_2dt = a0 * callLeavesDt[5] + a1 * callLeavesDt[6] + a2 * callLeavesDt[7];
		callOutput.charm = oneOverHalfDt * (delta_2dt - callOutput.delta);
	}

	if (calculatePut)
	{
		putOutput.delta = a0 * putData.payoff_i[(input.N >> 1) - 1] + a1 * putData.payoff_i[input.N >> 1] + a2 * putData.payoff_i[(input.N >> 1) + 1];
		putOutput.gamma = 2.0 * (b0 * putData.payoff_i[(input.N >> 1) - 1] + b1 * putData.payoff_i[input.N >> 1] + b2 * putData.payoff_i[(input.N >> 1) + 1]);

		putOutput.theta  = oneOverHalfDt  * (putLeavesDt[7]                        - putLeavesDt[1]);
		putOutput.theta2 = oneOverDt2 *     (putLeavesDt[7] - 2.0 * putLeavesDt[4] + putLeavesDt[1]);

		const double delta_2dt = a0 * putLeavesDt[5] + a1 * putLeavesDt[6] + a2 * putLeavesDt[7];
		putOutput.charm = oneOverHalfDt * (delta_2dt - putOutput.delta);
	}
}

}
