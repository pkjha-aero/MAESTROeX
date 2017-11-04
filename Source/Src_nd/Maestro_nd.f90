
subroutine ca_network_init() bind(C, name="ca_network_init")

  use network, only: network_init

  call network_init()

end subroutine ca_network_init


subroutine ca_get_num_spec(nspec_out) &
     bind(C, name="ca_get_num_spec")

  use network, only: nspec
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  integer, intent(out) :: nspec_out

  nspec_out = nspec

end subroutine ca_get_num_spec


subroutine ca_get_spec_names(spec_names,ispec,len) &
     bind(C, name="ca_get_spec_names")

  use network, only: nspec, short_spec_names
  use amrex_fort_module, only: rt => amrex_real

  implicit none

  integer, intent(in   ) :: ispec
  integer, intent(inout) :: len
  integer, intent(inout) :: spec_names(len)

  ! Local variables
  integer   :: i

  len = len_trim(short_spec_names(ispec+1))

  do i = 1,len
     spec_names(i) = ichar(short_spec_names(ispec+1)(i:i))
  end do

end subroutine ca_get_spec_names

subroutine ca_set_method_params(Density,Enthalpy,FirstSpec,Temperature,Pressure) &
                                bind(C, name="ca_set_method_params")

  use model_parser_module
  use meth_params_module
  use network, only : nspec
  use eos_module, only: eos_init

  implicit none

  integer, intent(in) :: Density, Enthalpy, FirstSpec, Temperature, Pressure

  integer :: i
  integer :: ioproc

  !---------------------------------------------------------------------
  ! conserved state components
  !---------------------------------------------------------------------

  RHO = Density
  RHOH = Enthalpy
  RHOX = FirstSpec
  TEMP = Temperature
  PRES = Pressure
  NSTATE = nspec + 4

  VELX = 1
  VELY = 2
  VELZ = 3

  !---------------------------------------------------------------------
  ! other initializations
  !---------------------------------------------------------------------

  ! This is a routine which links to the C++ ParallelDescriptor class

  call bl_pd_is_ioproc(ioproc)

  !---------------------------------------------------------------------
  ! safety checks
  !---------------------------------------------------------------------

  if (small_dens <= 0.d0) then
      if (ioproc == 1) then
         call bl_warning("Warning:: small_dens has not been set, defaulting to 1.d-200.")
      endif
      small_dens = 1.d-200
   endif

   if (small_temp <= 0.d0) then
     if (ioproc == 1) then
        call bl_warning("Warning:: small_temp has not been set, defaulting to 1.d-200.")
      endif
      small_temp = 1.d-200
   endif

  ! Note that the EOS may modify our choices because of its
  ! internal limitations, so the small_dens and small_temp
  ! may be modified coming back out of this routine.

  call eos_init(small_dens, small_temp)

  call read_model_file(model_file)

end subroutine ca_set_method_params
