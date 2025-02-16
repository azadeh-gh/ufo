/*
 * (C) Copyright 2019 UCAR
 *
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0.
 */

#include "ufo/filters/obsfunctions/HydrometeorCheckAMSUA.h"

#include <cmath>

#include <algorithm>
#include <set>
#include <string>
#include <vector>

#include "ioda/ObsDataVector.h"
#include "oops/util/IntSetParser.h"
#include "oops/util/missingValues.h"
#include "ufo/filters/ObsFilterData.h"
#include "ufo/filters/Variable.h"
#include "ufo/utils/Constants.h"


namespace ufo {

static ObsFunctionMaker<HydrometeorCheckAMSUA> makerHydrometeorCheckAMSUA_("HydrometeorCheckAMSUA");

// -----------------------------------------------------------------------------

HydrometeorCheckAMSUA::HydrometeorCheckAMSUA(const eckit::LocalConfiguration & conf)
  : invars_() {
  // Initialize options
  options_.deserialize(conf);

  // Get channels from options
  std::set<int> channelset = oops::parseIntSet(options_.channelList);
  std::copy(channelset.begin(), channelset.end(), std::back_inserter(channels_));
  ASSERT(channels_.size() > 0);

  // Get test groups from options
  const std::string &biastermgrp = options_.testBiasTerm.value();
  const std::string &biasgrp = options_.testBias.value();
  const std::string &hofxgrp = options_.testHofX.value();

  // Include required variables from ObsDiag
  invars_ += Variable("ObsDiag/brightness_temperature_jacobian_surface_emissivity", channels_);
  invars_ += Variable("ObsDiag/brightness_temperature_assuming_clear_sky", channels_);

  // Include list of required data from ObsSpace
  invars_ += Variable("ObsValue/brightnessTemperature", channels_);
  invars_ += Variable(biasgrp+"/brightnessTemperature", channels_);
  invars_ += Variable(hofxgrp+"/brightnessTemperature", channels_);
  invars_ += Variable(biastermgrp+"/constant", channels_);
  invars_ += Variable(biastermgrp+"/scan_angle_order_4", channels_);
  invars_ += Variable(biastermgrp+"/scan_angle_order_3", channels_);
  invars_ += Variable(biastermgrp+"/scan_angle_order_2", channels_);
  invars_ += Variable(biastermgrp+"/scan_angle", channels_);

  // Include list of required data from GeoVaLs
  invars_ += Variable("GeoVaLs/water_area_fraction");

  // Include list of required data from ObsFunction
  const Variable &obserrfunc = options_.obserrFunction.value();
  invars_ += obserrfunc;

  const Variable &clwretfunc = options_.clwretFunction.value();
  invars_ += clwretfunc;
}

// -----------------------------------------------------------------------------

HydrometeorCheckAMSUA::~HydrometeorCheckAMSUA() {}

// -----------------------------------------------------------------------------

void HydrometeorCheckAMSUA::compute(const ObsFilterData & in,
                                  ioda::ObsDataVector<float> & out) const {
  // Get dimensions
  size_t nlocs = in.nlocs();
  size_t nchans = channels_.size();

  // Set channel index
  int ich238 = 0, ich314 = 1, ich503 = 2, ich528 = 3, ich536 = 4;
  int ich544 = 5, ich549 = 6, ich890 = 14;

  // Get test groups from options
  const std::string &biastermgrp = options_.testBiasTerm.value();
  const std::string &biasgrp = options_.testBias.value();
  const std::string &hofxgrp = options_.testHofX.value();

  // Get clear-sky observation error from options
  const std::vector<float> &obserr0 = options_.obserrClearSky.value();

  // Get area fraction of each surface type
  std::vector<float> water_frac(nlocs);
  in.get(Variable("GeoVaLs/water_area_fraction"), water_frac);

  // Get surface temperature jacobian
  std::vector<std::vector<float>> dbtde(nchans, std::vector<float>(nlocs));
  for (size_t ichan = 0; ichan < nchans; ++ichan) {
    in.get(Variable("ObsDiag/brightness_temperature_jacobian_surface_emissivity", channels_)[ichan],
           dbtde[ichan]);
  }

  // Get HofX for clear-sky simulation
  std::vector<float> hofxclr536(nlocs);
  in.get(Variable("ObsDiag/brightness_temperature_assuming_clear_sky", channels_)[ich536],
         hofxclr536);

  // Get ObsBiasTerm: constant term for 23.8GHz channel
  std::vector<float> bias_const238(nlocs);
  in.get(Variable(biastermgrp+"/constant", channels_)[ich238], bias_const238);

  // Get ObsBiasTerm: scan angle terms for 23.8GHz channel
  size_t nangs = 4;
  std::vector<float> values(nlocs);
  std::vector<std::string> scanterms(nangs);
  std::vector<float> bias_scanang238(nlocs);
  scanterms[0] = biastermgrp+"/scan_angle_order_4";
  scanterms[1] = biastermgrp+"/scan_angle_order_3";
  scanterms[2] = biastermgrp+"/scan_angle_order_2";
  scanterms[3] = biastermgrp+"/scan_angle";
  for (size_t iang = 0; iang < nangs; ++iang) {
    in.get(Variable(scanterms[iang], channels_)[ich238], values);
    for (size_t iloc = 0; iloc < nlocs; ++iloc) {
      bias_scanang238[iloc] = bias_scanang238[iloc] + values[iloc];
    }
  }

  // Calculate bias-corrected innovation: Observation - HofX (HofX includes bias correction)
  std::vector<std::vector<float>> btobs(nchans, std::vector<float>(nlocs));
  // Read bias since it's used for correcting clear-sky simulated values below
  std::vector<std::vector<float>> bias(nchans, std::vector<float>(nlocs));
  std::vector<std::vector<float>> innov(nchans, std::vector<float>(nlocs));
  std::vector<float> hofx(nlocs);
  for (size_t ichan = 0; ichan < nchans; ++ichan) {
    in.get(Variable("ObsValue/brightnessTemperature", channels_)[ichan], btobs[ichan]);
    in.get(Variable(biasgrp+"/brightnessTemperature", channels_)[ichan], bias[ichan]);
    in.get(Variable(hofxgrp+"/brightnessTemperature", channels_)[ichan], hofx);
    for (size_t iloc = 0; iloc < nlocs; ++iloc) {
      innov[ichan][iloc] = btobs[ichan][iloc] - hofx[iloc];
    }
  }

  // Get all-sky observation error from ObsFunction
  const Variable &obserrvar = options_.obserrFunction.value();
  ioda::ObsDataVector<float> obserr(in.obsspace(), obserrvar.toOopsVariables());
  in.get(obserrvar, obserr);

  // Get CLW retrieval based on observation from ObsFunction
  const Variable &clwretvar = options_.clwretFunction.value();
  ioda::ObsDataVector<float> clwobs(in.obsspace(), clwretvar.toOopsVariables());
  in.get(clwretvar, clwobs);

  // Set parameters
  float w1f6 = 1.0/10.0, w2f6 = 1.0/0.80;
  float w1f4 = 1.0/0.30, w2f4 = 1.0/1.80;

  std::vector<std::vector<int>> affected_channels(nchans, std::vector<int>(nlocs));
  // Loop over locations
  // Combined cloud-precipitation-surface checks
  for (size_t iloc = 0; iloc < nlocs; ++iloc) {
    // Initialization
    for (size_t ich = 0; ich < nchans; ++ich) {
      affected_channels[ich][iloc] = 0;
    }

    // Window channel sanity check
    // If any of the window channels is bad, skip all window channels
    // List of surface sensitivity channels
    std::vector<float> OmFs{std::abs(innov[ich238][iloc]), std::abs(innov[ich314][iloc]),
                            std::abs(innov[ich503][iloc]), std::abs(innov[ich528][iloc]),
                            std::abs(innov[ich536][iloc]), std::abs(innov[ich544][iloc]),
                            std::abs(innov[ich890][iloc])};
    bool result = false;
    result = any_of(OmFs.begin(), OmFs.end(), [](float x){
               return (x > 200.0 || x == util::missingValue(1.0f));});

    if (result) {
      for (size_t ich = ich238; ich <= ich544; ++ich) {
        affected_channels[ich][iloc] = 1;
      }
      affected_channels[ich890][iloc] = 1;
    } else {
      // Calculate scattering effect
      float clwx = 0.6;
      float dsval = 0.8;
      if (water_frac[iloc] > 0.99) {
        clwx = 0.0;
        float btobsbc238 = btobs[ich238][iloc] - bias_const238[iloc]
                                               - bias_scanang238[iloc];
        dsval = ((2.410 - 0.0098 * btobsbc238) * innov[ich238][iloc] +
                  0.454 * innov[ich314][iloc] - innov[ich890][iloc]) * w1f6;
        dsval = std::max(static_cast<float>(0.0), dsval);
      }
      float factch4 = pow(clwx, 2) + pow(innov[ich528][iloc] * w2f4, 2);
      float factch6 = pow(dsval, 2) + pow(innov[ich544][iloc] * w2f6, 2);

      // Hydrometeor check over water surface
      if (water_frac[iloc] > 0.99) {
        // Calculate cloud effect from 53.6 GHz (Channel 4)
        float cldeff_obs536 = btobs[ich536][iloc] - hofxclr536[iloc] - bias[ich536][iloc];

        // Cloud water retrieval sanity check
        if (clwobs[0][iloc] > 999.0) {
          for (size_t ich = ich238; ich <= ich544; ++ich) {
            affected_channels[ich][iloc] = 1;
          }
          affected_channels[ich890][iloc] = 1;
        }

        // Precipitation check (factch6)
        if (factch6 >= 1.0) {
          for (size_t ich = ich238; ich <= ich544; ++ich) {
            affected_channels[ich][iloc] = 1;
          }
          affected_channels[ich890][iloc] = 1;
        // Scattering check (ch5 cloud effect)
        } else if (cldeff_obs536 < -0.5) {
          for (size_t ich = ich238; ich <= ich544; ++ich) {
            affected_channels[ich][iloc] = 1;
          }
          affected_channels[ich890][iloc] = 1;
        // Sensitivity of BT to the surface emissivity check
        } else {
          float thrd238 = 0.025, thrd314 = 0.015, thrd503 = 0.030, thrd890 = 0.030;
          float de238 = 0.0, de314 = 0.0, de503 = 0.0, de890 = 0.0;
          float dbtde238 = dbtde[ich238][iloc];
          float dbtde314 = dbtde[ich314][iloc];
          float dbtde503 = dbtde[ich503][iloc];
          float dbtde890 = dbtde[ich890][iloc];
          if (dbtde238 != 0.0) de238 = std::abs(innov[ich238][iloc]) / dbtde238 *
                                       (obserr0[ich238] / obserr[ich238][iloc]) *
                                       (1.0 - std::max(1.0, 10.0*clwobs[0][iloc]));
          if (dbtde314 != 0.0) de314 = std::abs(innov[ich314][iloc]) / dbtde314 *
                                       (obserr0[ich314] / obserr[ich314][iloc]) *
                                       (1.0 - std::max(1.0, 10.0*clwobs[0][iloc]));
          if (dbtde503 != 0.0) de503 = std::abs(innov[ich503][iloc]) / dbtde503 *
                                       (obserr0[ich503] / obserr[ich503][iloc]) *
                                       (1.0 - std::max(1.0, 10.0*clwobs[0][iloc]));
          if (dbtde890 != 0.0) de890 = std::abs(innov[ich890][iloc]) / dbtde890 *
                                       (obserr0[ich890] / obserr[ich890][iloc]) *
                                       (1.0 - std::max(1.0, 10.0*clwobs[0][iloc]));
          bool qcemiss = false;
          qcemiss = de238 > thrd238 || de314 > thrd314 || de503 > thrd503 || de890 > thrd890;
          if (qcemiss) {
            for (size_t ich = ich238; ich <= ich536; ++ich) {
              affected_channels[ich][iloc] = 1;
            }
            affected_channels[ich890][iloc] = 1;
          }
        }
      } else {
        // Hydrometeor check over non-water (land/sea ice/snow) surface
        // Precipitation check (factch6)
        if (factch6 >= 1.0) {
          for (size_t ich = ich238; ich <= ich544; ++ich) {
            affected_channels[ich][iloc] = 1;
          }
          affected_channels[ich890][iloc] = 1;
        // Thick cloud check (factch4)
        } else if (factch4 > 0.5) {
          for (size_t ich = ich238; ich <= ich536; ++ich) {
            affected_channels[ich][iloc] = 1;
          }
          affected_channels[ich890][iloc] = 1;
        // Sensitivity of BT to the surface emissivity check
        } else {
          float thrd238 = 0.020, thrd314 = 0.015, thrd503 = 0.035, thrd890 = 0.015;
          float de238 = 0.0, de314 = 0.0, de503 = 0.0, de890 = 0.0;
          float dbtde238 = dbtde[ich238][iloc];
          float dbtde314 = dbtde[ich314][iloc];
          float dbtde503 = dbtde[ich503][iloc];
          float dbtde890 = dbtde[ich890][iloc];
          if (dbtde238 != 0.0) de238 = std::abs(innov[ich238][iloc]) / dbtde238;
          if (dbtde314 != 0.0) de314 = std::abs(innov[ich314][iloc]) / dbtde314;
          if (dbtde503 != 0.0) de503 = std::abs(innov[ich503][iloc]) / dbtde503;
          if (dbtde890 != 0.0) de890 = std::abs(innov[ich890][iloc]) / dbtde890;
          bool qcemiss = false;
          qcemiss = de238 > thrd238 || de314 > thrd314 || de503 > thrd503 || de890 > thrd890;
          if (qcemiss) {
            for (size_t ich = ich238; ich <= ich536; ++ich) {
              affected_channels[ich][iloc] = 1;
            }
            affected_channels[ich890][iloc] = 1;
          }
        }
      // surface type
      }
    // window channel sanity check
    }
  // loop over locations
  }
  // Output
  for (size_t ichan = 0; ichan < nchans; ++ichan) {
    for (size_t iloc = 0; iloc < nlocs; ++iloc) {
      out[ichan][iloc] = affected_channels[ichan][iloc];
    }
  }
}

// -----------------------------------------------------------------------------

const ufo::Variables & HydrometeorCheckAMSUA::requiredVariables() const {
  return invars_;
}

// -----------------------------------------------------------------------------

}  // namespace ufo
