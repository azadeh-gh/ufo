/*
 * (C) British Crown Copyright 2022 Met Office
 * 
 * This software is licensed under the terms of the Apache Licence Version 2.0
 * which can be obtained at http://www.apache.org/licenses/LICENSE-2.0. 
 */

#ifndef UFO_OPERATORS_SCATWIND_NEUTRALMETOFFICE_OBSSCATWINDNEUTRALMETOFFICETLAD_H_
#define UFO_OPERATORS_SCATWIND_NEUTRALMETOFFICE_OBSSCATWINDNEUTRALMETOFFICETLAD_H_

#include <ostream>
#include <string>

#include "oops/base/Variables.h"
#include "oops/util/ObjectCounter.h"
#include "ufo/LinearObsOperatorBase.h"
#include "ufo/operators/scatwind/NeutralMetOffice/ObsScatwindNeutralMetOfficeTLAD.interface.h"

// Forward declarations
namespace ioda {
  class ObsSpace;
  class ObsVector;
}

namespace ufo {
  class GeoVaLs;
  class ObsDiagnostics;

/// Configuration options recognized by the ObsScatwindNeutralMetOffice operator.
class ObsScatwindNeutralMetOfficeParameters : public ObsOperatorParametersBase {
  OOPS_CONCRETE_PARAMETERS(ObsScatwindNeutralMetOfficeParameters, ObsOperatorParametersBase)
};

// -----------------------------------------------------------------------------

/// ScatwindNeutralMetOffice observation operator
class ObsScatwindNeutralMetOfficeTLAD : public LinearObsOperatorBase,
                      private util::ObjectCounter<ObsScatwindNeutralMetOfficeTLAD> {
 public:
  /// The type of parameters accepted by the constructor of this operator.
  /// This typedef is used by the ObsOperatorFactory.
  typedef ObsScatwindNeutralMetOfficeParameters Parameters_;

  static const std::string classname() {return "ufo::ObsScatwindNeutralMetOfficeTLAD";}

  ObsScatwindNeutralMetOfficeTLAD(const ioda::ObsSpace &, const Parameters_ &);
  virtual ~ObsScatwindNeutralMetOfficeTLAD();

// Obs Operator
  void setTrajectory(const GeoVaLs &, ObsDiagnostics &) override;
  void simulateObsTL(const GeoVaLs &, ioda::ObsVector &) const override;
  void simulateObsAD(GeoVaLs &, const ioda::ObsVector &) const override;

// Other
  const oops::Variables & requiredVars() const override {return varin_;}

  int & toFortran() {return keyOperScatwindNeutralMetOffice_;}
  const int & toFortran() const {return keyOperScatwindNeutralMetOffice_;}

 private:
  void print(std::ostream &) const override;
  F90hop keyOperScatwindNeutralMetOffice_;
  oops::Variables varin_;
  ObsScatwindNeutralMetOfficeParameters parameters_;
};

// -----------------------------------------------------------------------------

}  // namespace ufo

#endif  // UFO_OPERATORS_SCATWIND_NEUTRALMETOFFICE_OBSSCATWINDNEUTRALMETOFFICETLAD_H_
