/*
  Copyright 2017 SINTEF ICT, Applied Mathematics.
  Copyright 2017 Statoil ASA.

  This file is part of the Open Porous Media project (OPM).

  OPM is free software: you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation, either version 3 of the License, or
  (at your option) any later version.

  OPM is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with OPM.  If not, see <http://www.gnu.org/licenses/>.
*/


namespace Opm
{
    template<typename TypeTag>
    StandardWell<TypeTag>::
    StandardWell(const Well* well, const int time_step, const Wells* wells)
    : WellInterface<TypeTag>(well, time_step, wells)
    , perf_densities_(numberOfPerforations())
    , perf_pressure_diffs_(numberOfPerforations())
    , well_variables_(numWellEq) // the number of the primary variables
    {
        dune_B_.setBuildMode( Mat::row_wise );
        dune_C_.setBuildMode( Mat::row_wise );
        inv_dune_D_.setBuildMode( Mat::row_wise );
    }





    template<typename TypeTag>
    const std::vector<double>&
    StandardWell<TypeTag>::
    perfDensities() const
    {
        return perf_densities_;
    }





    template<typename TypeTag>
    std::vector<double>&
    StandardWell<TypeTag>::
    perfDensities()
    {
        return perf_densities_;
    }





    template<typename TypeTag>
    const std::vector<double>&
    StandardWell<TypeTag>::
    perfPressureDiffs() const
    {
        return perf_pressure_diffs_;
    }





    template<typename TypeTag>
    std::vector<double>&
    StandardWell<TypeTag>::
    perfPressureDiffs()
    {
        return perf_pressure_diffs_;
    }





    template<typename TypeTag>
    void
    StandardWell<TypeTag>::
    assembleWellEq(Simulator& ebos_simulator,
                   const double dt,
                   WellState& well_state,
                   bool only_wells)
    {
    }





    template<typename TypeTag>
    void StandardWell<TypeTag>::
    setWellVariables(const WellState& well_state)
    {
        const int nw = well_state.bhp().size();
        const int numComp = numComponents();
        for (int eqIdx = 0; eqIdx < numComp; ++eqIdx) {
            const unsigned int idx = nw * eqIdx + indexOfWell();
            assert( eqIdx < well_variables_.size() );
            assert( idx < well_state.wellSolutions().size() );

            well_variables_[eqIdx] = 0.0;
            well_variables_[eqIdx].setValue(well_state.wellSolutions()[idx]);
            well_variables_[eqIdx].setDerivative(numEq + eqIdx, 1.0);
        }
    }





    template<typename TypeTag>
    typename StandardWell<TypeTag>::EvalWell
    StandardWell<TypeTag>::
    getBhp() const
    {
        const WellControls* wc = wellControls();
        if (well_controls_get_current_type(wc) == BHP) {
            EvalWell bhp = 0.0;
            const double target_rate = well_controls_get_current_target(wc);
            bhp.setValue(target_rate);
            return bhp;
        } else if (well_controls_get_current_type(wc) == THP) {
            const int control = well_controls_get_current(wc);
            const double thp = well_controls_get_current_target(wc);
            const double alq = well_controls_iget_alq(wc, control);
            const int table_id = well_controls_iget_vfp(wc, control);
            EvalWell aqua = 0.0;
            EvalWell liquid = 0.0;
            EvalWell vapour = 0.0;
            EvalWell bhp = 0.0;
            double vfp_ref_depth = 0.0;

            const Opm::PhaseUsage& pu = phaseUsage();

            if (active()[ Water ]) {
                aqua = getQs(pu.phase_pos[ Water]);
            }
            if (active()[ Oil ]) {
                liquid = getQs(pu.phase_pos[ Oil ]);
            }
            if (active()[ Gas ]) {
                vapour = getQs(pu.phase_pos[ Gas ]);
            }
            if (wellType() == INJECTOR) {
                bhp = vfp_properties_->getInj()->bhp(table_id, aqua, liquid, vapour, thp);
                vfp_ref_depth = vfp_properties_->getInj()->getTable(table_id)->getDatumDepth();
            } else {
                bhp = vfp_properties_->getProd()->bhp(table_id, aqua, liquid, vapour, thp, alq);
                vfp_ref_depth = vfp_properties_->getProd()->getTable(table_id)->getDatumDepth();
            }

            // pick the density in the top layer
            const double rho = perf_densities_[0];
            // TODO: not sure whether it is always correct
            const double well_ref_depth = perfDepth()[0];
            const double dp = wellhelpers::computeHydrostaticCorrection(well_ref_depth, vfp_ref_depth, rho, gravity_);
            bhp -= dp;
            return bhp;
        }

        return well_variables_[XvarWell];
    }





    template<typename TypeTag>
    typename StandardWell<TypeTag>::EvalWell
    StandardWell<TypeTag>::
    getQs(const int phase) const
    {
        EvalWell qs = 0.0;

        const WellControls* wc = wellControls();
        const int np = numberOfPhases();
        const double target_rate = well_controls_get_current_target(wc);

        // TODO: we need to introduce numComponents() for StandardWell
        // assert(phase < numComponents());
        const auto pu = phaseUsage();

        // TODO: the formulation for the injectors decides it only work with single phase
        // surface rate injection control. Improvement will be required.
        if (wellType() == INJECTOR) {
            // TODO: adding the handling related to solvent
            /* if (has_solvent_ ) {
                // TODO: investigate whether the use of the comp_frac is justified.
                double comp_frac = 0.0;
                if (compIdx == solventCompIdx) { // solvent
                    comp_frac = wells().comp_frac[np*wellIdx + pu.phase_pos[ Gas ]] * wsolvent(wellIdx);
                } else if (compIdx == pu.phase_pos[ Gas ]) {
                    comp_frac = wells().comp_frac[np*wellIdx + compIdx] * (1.0 - wsolvent(wellIdx));
                } else {
                    comp_frac = wells().comp_frac[np*wellIdx + compIdx];
                }
                if (comp_frac == 0.0) {
                    return qs; //zero
                }

                if (well_controls_get_current_type(wc) == BHP || well_controls_get_current_type(wc) == THP) {
                    return comp_frac * well_variables_[nw*XvarWell + wellIdx];
                }

                qs.setValue(comp_frac * target_rate);
                return qs;
            } */
            const double comp_frac = compFrac()[phase];
            if (comp_frac == 0.0) {
                return qs;
            }

            if (well_controls_get_current_type(wc) == BHP || well_controls_get_current_type(wc) == THP) {
                return well_variables_[XvarWell];
            }
            qs.setValue(target_rate);
            return qs;
        }

        // Producers
        if (well_controls_get_current_type(wc) == BHP || well_controls_get_current_type(wc) == THP ) {
            return well_variables_[XvarWell] * wellVolumeFractionScaled(phase);
        }

        if (well_controls_get_current_type(wc) == SURFACE_RATE) {
            // checking how many phases are included in the rate control
            // to decide wheter it is a single phase rate control or not
            const double* distr = well_controls_get_current_distr(wc);
            int num_phases_under_rate_control = 0;
            for (int phase = 0; phase < np; ++phase) {
                if (distr[phase] > 0.0) {
                    num_phases_under_rate_control += 1;
                }
            }

            // there should be at least one phase involved
            assert(num_phases_under_rate_control > 0);

            // when it is a single phase rate limit
            if (num_phases_under_rate_control == 1) {

                // looking for the phase under control
                int phase_under_control = -1;
                for (int phase = 0; phase < np; ++phase) {
                    if (distr[phase] > 0.0) {
                        phase_under_control = phase;
                        break;
                    }
                }

                assert(phase_under_control >= 0);

                EvalWell wellVolumeFractionScaledPhaseUnderControl = wellVolumeFractionScaled(phase_under_control);
                // TODO: handling solvent related later
                /* if (has_solvent_ && phase_under_control == Gas) {
                    // for GRAT controlled wells solvent is included in the target
                    wellVolumeFractionScaledPhaseUnderControl += wellVolumeFractionScaled(solventCompIdx);
                } */

                if (phase == phase_under_control) {
                    /* if (has_solvent_ && phase_under_control == Gas) {
                        qs.setValue(target_rate * wellVolumeFractionScaled(Gas).value() / wellVolumeFractionScaledPhaseUnderControl.value() );
                        return qs;
                    } */
                    qs.setValue(target_rate);
                    return qs;
                }

                // TODO: not sure why the single phase under control will have near zero fraction
                const double eps = 1e-6;
                if (wellVolumeFractionScaledPhaseUnderControl < eps) {
                    return qs;
                }
                return (target_rate * wellVolumeFractionScaled(phase) / wellVolumeFractionScaledPhaseUnderControl);
            }

            // when it is a combined two phase rate limit, such like LRAT
            // we neec to calculate the rate for the certain phase
            if (num_phases_under_rate_control == 2) {
                EvalWell combined_volume_fraction = 0.;
                for (int p = 0; p < np; ++p) {
                    if (distr[p] == 1.0) {
                        combined_volume_fraction += wellVolumeFractionScaled(p);
                    }
                }
                return (target_rate * wellVolumeFractionScaled(phase) / combined_volume_fraction);
            }

            // TODO: three phase surface rate control is not tested yet
            if (num_phases_under_rate_control == 3) {
                return target_rate * wellSurfaceVolumeFraction(phase);
            }
        } else if (well_controls_get_current_type(wc) == RESERVOIR_RATE) {
            // ReservoirRate
            return target_rate * wellVolumeFractionScaled(phase);
        } else {
            OPM_THROW(std::logic_error, "Unknown control type for well " << name());
        }

        // avoid warning of condition reaches end of non-void function
        return qs;
    }






    template<typename TypeTag>
    typename StandardWell<TypeTag>::EvalWell
    StandardWell<TypeTag>::
    wellVolumeFractionScaled(const int phase) const
    {
        // TODO: we should be able to set the g for the well based on the control type
        // instead of using explicit code for g all the times
        const WellControls* wc = wellControls();
        if (well_controls_get_current_type(wc) == RESERVOIR_RATE) {
            const double* distr = well_controls_get_current_distr(wc);
            if (distr[phase] > 0.) {
                return wellVolumeFraction(phase) / distr[phase];
            } else {
                // TODO: not sure why return EvalWell(0.) causing problem here
                // Probably due to the wrong Jacobians.
                return wellVolumeFraction(phase);
            }
        }
        std::vector<double> g = {1,1,0.01};
        return (wellVolumeFraction(phase) / g[phase]);
    }





    template<typename TypeTag>
    typename StandardWell<TypeTag>::EvalWell
    StandardWell<TypeTag>::
    wellVolumeFraction(const int phase) const
    {
        if (phase == Water) {
            return well_variables_[WFrac];
        }

        if (phase == Gas) {
            return well_variables_[GFrac];
        }

        // Oil fraction
        EvalWell well_fraction = 1.0;
        if (active()[Water]) {
            well_fraction -= well_variables_[WFrac];
        }

        if (active()[Gas]) {
            well_fraction -= well_variables_[GFrac];
        }
        return well_fraction;
    }





    template<typename TypeTag>
    typename StandardWell<TypeTag>::EvalWell
    StandardWell<TypeTag>::
    wellSurfaceVolumeFraction(const int phase) const
    {
        EvalWell sum_volume_fraction_scaled = 0.;
        const int np = numberOfPhases();
        for (int p = 0; p < np; ++p) {
            sum_volume_fraction_scaled += wellVolumeFractionScaled(p);
        }

        assert(sum_volume_fraction_scaled.value() != 0.);

        return wellVolumeFractionScaled(phase) / sum_volume_fraction_scaled;
     }





    template<typename TypeTag>
    typename StandardWell<TypeTag>::EvalWell
    StandardWell<TypeTag>::
    extendEval(const Eval& in) const
    {
        EvalWell out = 0.0;
        out.setValue(in.value());
        for(int eqIdx = 0; eqIdx < numEq;++eqIdx) {
            out.setDerivative(eqIdx, in.derivative(flowToEbosPvIdx(eqIdx)));
        }
        return out;
    }





    template<typename TypeTag>
    void
    StandardWell<TypeTag>::
    computePerfRate(const IntensiveQuantities& intQuants,
                    const std::vector<EvalWell>& mob_perfcells_dense,
                    const EvalWell& bhp, const double& cdp,
                    const bool& allow_cf, std::vector<EvalWell>& cq_s) const
    {




    }

}
