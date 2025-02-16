! (C) Copyright 2018 UCAR
! 
! This software is licensed under the terms of the Apache Licence Version 2.0
! which can be obtained at http://www.apache.org/licenses/LICENSE-2.0. 

!> Fortran module to perform linear interpolation

module vert_interp_mod

use, intrinsic :: iso_c_binding
use kinds, only: kind_real
use missing_values_mod

implicit none
public

contains

! ------------------------------------------------------------------------------

subroutine vert_interp_weights(nlev,obl,vec,wi,wf)

implicit none
integer,         intent(in ) :: nlev       !Number of model levels
real(kind_real), intent(in ) :: obl        !Observation location
real(kind_real), intent(in ) :: vec(nlev)  !Structured vector of grid points
integer,         intent(out) :: wi         !Index for interpolation
real(kind_real), intent(out) :: wf         !Weight for interpolation

integer         :: k
real(kind_real) :: missing

missing = missing_value(obl)

! If the observation is missing then set both the index and weight to missing.
if (obl == missing) then
   wi = missing_value(nlev)
   wf = missing
   return
end if

if (vec(1) < vec(nlev)) then !Pressure increases with index

  if (obl < vec(1)) then
     wi = 1
     wf = 1.0
  elseif (obl > vec(nlev)) then
     wi = nlev - 1
     wf = 0.0
  else
     do k = 1,nlev-1
        if (obl >= vec(k) .and. obl <= vec(k+1)) then
           wi = k
        endif
     enddo
     wf = (vec(wi+1) - obl)/(vec(wi+1) - vec(wi))
  endif

else !Pressure decreases with index

  if (obl > vec(1)) then
     wi = 1
     wf = 1.0
  elseif (obl < vec(nlev)) then
     wi = nlev - 1
     wf = 0.0
  else
     do k = 1,nlev-1
        if (obl >= vec(k+1) .and. obl <= vec(k)) then
           wi = k
        endif
     enddo
     wf = (vec(wi+1) - obl)/(vec(wi+1) - vec(wi))
  endif

endif

end subroutine vert_interp_weights

! ------------------------------------------------------------------------------

subroutine vert_interp_apply(nlev, fvec, f, wi, wf) 

implicit none
integer,         intent(in ) :: nlev        !Number of model levels
real(kind_real), intent(in ) :: fvec(nlev)  !Field at grid points
integer,         intent(in ) :: wi          !Index for interpolation
real(kind_real), intent(in ) :: wf          !Weight for interpolation
real(kind_real), intent(out) :: f           !Output at obs location using linear interp

if (wi == missing_value(nlev)) then
  f = missing_value(f)
elseif (fvec(wi) == missing_value(f) .or. fvec(wi+1) == missing_value(f)) then
  f = missing_value(f)
else
  f = fvec(wi)*wf + fvec(wi+1)*(1.0-wf)
endif

end subroutine vert_interp_apply

! ------------------------------------------------------------------------------

subroutine vert_interp_apply_tl(nlev, fvec_tl, f_tl, wi, wf) 

implicit none
integer,         intent(in)  :: nlev
real(kind_real), intent(in)  :: fvec_tl(nlev)
integer,         intent(in)  :: wi
real(kind_real), intent(in)  :: wf
real(kind_real), intent(out) :: f_tl

if (wi == missing_value(nlev)) then
  f_tl = missing_value(f_tl)
elseif (fvec_tl(wi) == missing_value(f_tl) .or. fvec_tl(wi+1) == missing_value(f_tl)) then
  f_tl = missing_value(f_tl)
else
  f_tl = fvec_tl(wi)*wf + fvec_tl(wi+1)*(1.0_kind_real-wf)
endif

end subroutine vert_interp_apply_tl

! ------------------------------------------------------------------------------

subroutine vert_interp_apply_ad(nlev, fvec_ad, f_ad, wi, wf) 

implicit none
integer,         intent(in)    :: nlev
real(kind_real), intent(inout) :: fvec_ad(nlev)
integer,         intent(in)    :: wi
real(kind_real), intent(in)    :: wf
real(kind_real), intent(in)    :: f_ad
real(kind_real) :: missing

missing = missing_value(missing)

! Do not modify the adjoint if the weight index is missing.
! This occurs when the observed vertical coordinate is missing.
if (wi == missing_value(nlev)) return

if (fvec_ad(wi) == missing .or. f_ad == missing) then
  fvec_ad(wi  ) = 0.0_kind_real
else
  fvec_ad(wi  ) = fvec_ad(wi  ) + f_ad*wf
endif
if (fvec_ad(wi+1) == missing .or. f_ad == missing) then
  fvec_ad(wi+1) = 0.0_kind_real
else
  fvec_ad(wi+1) = fvec_ad(wi+1) + f_ad*(1.0_kind_real-wf)
endif

end subroutine vert_interp_apply_ad

! ------------------------------------------------------------------------------

subroutine nearestneighbor_interp_index(nlev,obl,vec,idx)

implicit none
integer,         intent(in ) :: nlev       !Number of model levels
real(kind_real), intent(in ) :: obl        !Observation location
real(kind_real), intent(in ) :: vec(nlev)  !Structured vector of grid points
integer,         intent(out) :: idx        !Index for interpolation

integer         :: k
real(kind_real) :: missing

missing = missing_value(obl)

! If the observation is missing then set both the index and weight to missing.
if (obl == missing) then
   idx= missing_value(nlev)
   return
end if

if (vec(1) < vec(nlev)) then !increases with index

  if (obl <= vec(1)) then
     idx = 1
  else
     idx = nlev
     do k = 2, nlev
        if (obl <= vec(k)) then
           if((obl - vec(k-1)) <= (vec(k)-obl)) then
             idx = k - 1
           else
             idx = k
           endif
           exit
        endif
     enddo
  endif

else !decreases with index

  if (obl >= vec(1)) then
     idx = 1
  else
     idx = nlev
     do k = 2, nlev
        if (obl >= vec(k)) then
           if((vec(k-1)-obl) <= (obl-vec(k))) then
             idx = k - 1
           else
             idx = k
           endif
           exit
        endif
     enddo
  endif

endif

end subroutine nearestneighbor_interp_index

! ------------------------------------------------------------------------------

subroutine nearestneighbor_interp_apply(nlev, fvec, f, idx)

implicit none
integer,         intent(in ) :: nlev        !Number of model levels
real(kind_real), intent(in ) :: fvec(nlev)  !Field at grid points
integer,         intent(in ) :: idx         !Index for interpolation
real(kind_real), intent(out) :: f           !Output at obs location using linear interp

if (idx == missing_value(nlev)) then
  f = missing_value(f)
else
  f = fvec(idx)
endif

end subroutine nearestneighbor_interp_apply

! ------------------------------------------------------------------------------

subroutine nearestneighbor_interp_apply_tl(nlev, fvec_tl, f_tl, idx)

implicit none
integer,         intent(in)  :: nlev
real(kind_real), intent(in)  :: fvec_tl(nlev)
integer,         intent(in)  :: idx
real(kind_real), intent(out) :: f_tl

if (idx== missing_value(nlev)) then
  f_tl = missing_value(f_tl)
else if (fvec_tl(idx) == missing_value(f_tl)) then
  f_tl = missing_value(f_tl)
else
  f_tl = fvec_tl(idx)
endif

end subroutine nearestneighbor_interp_apply_tl

! ------------------------------------------------------------------------------

subroutine nearestneighbor_interp_apply_ad(nlev, fvec_ad, f_ad, idx)

implicit none
integer,         intent(in)    :: nlev
real(kind_real), intent(inout) :: fvec_ad(nlev)
integer,         intent(in)    :: idx
real(kind_real), intent(in)    :: f_ad
real(kind_real) :: missing

missing = missing_value(missing)

! Do not modify the adjoint if the weight index is missing.
! This occurs when the observed vertical coordinate is missing.
if (idx == missing_value(nlev)) return

if (fvec_ad(idx) == missing .or. f_ad == missing) then
  fvec_ad(idx) = 0.0_kind_real
else
  fvec_ad(idx) = fvec_ad(idx) + f_ad
endif

end subroutine nearestneighbor_interp_apply_ad

! ------------------------------------------------------------------------------

end module vert_interp_mod
