 program main

  use euler
  implicit none
  real(kind=8) :: t_0, y_0, t_f
  Character(len = 13) :: f
  Character(len = 2) :: filenum
  character(len=1024) :: filename
  integer :: nCount
  nCount = 8
  t_0 = 0.
  y_0 = -2.
  t_f = 10.
  
  ! Calls the Euler's method subroutine for each N value and saves the results to a file.
  do while (nCount < 65)
  write(filenum,'(I2.2)') nCount
  f = 'output_'//trim(filenum)//'.dat'
  
  call eulers_method(t_0,y_0,t_f,nCount,f)
  
  nCount = nCount*2
  
  end do
   
 end program main
