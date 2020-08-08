module euler

contains 

  ! Original function derivative
  real(kind=8) function dydt(t,y)
  implicit none
  real(kind=8), intent(in) :: t,y
  dydt = (2*t) / (y*(1+(t**2)))
  end function dydt
 
  
  ! Subroutine to use Euler's method to estimate the solution of the first order differential equation.
  subroutine eulers_method(t_0, y_0, t_f, N, file_name)
  
  implicit none
  real(kind=8), intent(in) :: t_0, y_0, t_f
  integer, intent(in) :: N
  !real(kind=8), intent(out) :: file_name
  Character(len = 13), intent(out) :: file_name
  integer :: i
  real(kind=8) :: t, y, func, y_value, h
  
  t = t_0
  y = y_0
  y_value = y_0
  
  h = t_f/(N-1)
  open(1, file = file_name, status = 'new')
  
  do i = 0, N-1
  
	func = dydt(t,y)
	y_value = y + h * func
	
	
	write(1, '("  ", F19.16, "    ", F19.16 )' ) t, y_value

	
	t = t + h
	y = y_value
	
  end do
  close(1)
  
  end subroutine eulers_method

  
  

end module euler




