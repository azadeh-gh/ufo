/*
 * (C) Copyright 2017-2021 UCAR
 * 
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0. 
 */

#ifndef UFO_OBSBIAS_H_
#define UFO_OBSBIAS_H_

#include <Eigen/Core>

#include <memory>
#include <string>
#include <vector>

#include "oops/base/Variables.h"
#include "oops/util/ObjectCounter.h"
#include "oops/util/Printable.h"

#include "ufo/ObsBiasParameters.h"
#include "ufo/predictors/PredictorBase.h"

namespace ioda {
  class ObsSpace;
}

namespace ufo {
  class ObsBiasIncrement;

/// Class to handle observation bias correction coefficients
/// \details contains information on what predictors are used for bias
///          correction application
class ObsBias : public util::Printable,
                private util::ObjectCounter<ObsBias> {
 public:
  typedef ObsBiasParameters Parameters_;

  static const std::string classname() {return "ufo::ObsBias";}

  ObsBias(ioda::ObsSpace &, const Parameters_ &);
  ObsBias(const ObsBias &, const bool);

  ObsBias & operator+=(const ObsBiasIncrement &);
  ObsBias & operator=(const ObsBias &);

  /// Read bias correction coefficients from file
  void read(const Parameters_ &);
  void write(const Parameters_ &) const;
  double norm() const;
  std::size_t size() const {return biascoeffs_.size();}

  /// Return the coefficient of predictor \p jpred for variable \p jvar.
  ///
  /// Note: \p jpred may be the index of a static or a variable predictor.
  double operator()(size_t jpred, size_t jvar) const {
    return jpred < numStaticPredictors_ ?
           1.0 : biascoeffs_[index(jpred - numStaticPredictors_, jvar)];
  }

  /// Return bias correction coefficients (for *variable* predictors)
  const Eigen::VectorXd & data() const {return biascoeffs_;}

  // Required variables
  const oops::Variables & requiredVars() const {return geovars_;}
  const oops::Variables & requiredHdiagnostics() const {return hdiags_;}
  const std::vector<std::string> & requiredPredictors() const {return prednames_;}

  /// Return a reference to the vector of all (static and variable) predictors.
  const Predictors & predictors() const {return predictors_;}

  /// Return the vector of variable predictors.
  std::vector<std::shared_ptr<const PredictorBase>> variablePredictors() const;

  /// Return the list of simulated variables.
  const oops::Variables & simVars() const {return vars_;}

  /// Return the indices of variables (or channels) that don't need bias correction
  const std::vector<int> & varIndexNoBC() const {return varIndexNoBC_;}

  /// Set all variable predictors coefficients to zero (used in the test)
  void zero();

  // Operator
  operator bool() const {
    return (numStaticPredictors_ > 0 || numVariablePredictors_ > 0) && vars_.size() > 0;
  }

 private:
  void print(std::ostream &) const override;

  /// index in the flattened biascoeffs_ for predictor \p jpred and variable \p jvar
  size_t index(size_t jpred, size_t jvar) const {return jvar*numVariablePredictors_ + jpred;}

  void initPredictor(const PredictorParametersWrapper &params);

  /// bias correction coefficients (npredictors x nprimitivevariables)
  Eigen::VectorXd biascoeffs_;

  /// bias correction predictors
  Predictors predictors_;
  /// predictor names
  std::vector<std::string> prednames_;
  /// number of static predictors (i.e. predictors with fixed coefficients all equal to 1)
  std::size_t numStaticPredictors_;
  /// number of variable predictors (i.e. predictors with variable coefficients)
  std::size_t numVariablePredictors_;

  /// list of simulated variables
  oops::Variables vars_;
  /// indices of variables (or channels) that don't need bias correction
  std::vector<int> varIndexNoBC_;

  /// Variables that need to be requested from the model (for computation of predictors)
  oops::Variables geovars_;
  /// Diagnostics that need to be requested from the obs operator (for computation of predictors)
  oops::Variables hdiags_;

  /// MPI rank, used to determine whether the task should output bias coeffs to a file
  size_t rank_;
};

// -----------------------------------------------------------------------------

}  // namespace ufo

#endif  // UFO_OBSBIAS_H_
