COMP 371 - Computer Graphics - Assignment 2
Michael Bilinsky 26992358

This app allows the user to specify points and their tangeants, then
	calculates hermite splines and animates a triangle along the splines

Created with Visual Studio 2015

Controls:
Arrow keys to move the camera around
P and L to toggle between Points and Lines
Enter to compute splines
Backspace to reset the points and start the program over again

Extra Credit:
In the spline calculation, the curvature of the line is considered in order to limit the number of points computed
along straight segments of the splines.

Movement Speed:
In testing, interaction with the program has different sensitivities on different machines.
To modify the rate movement, change the TRIANGLE_SPLINE_FOLLOW_SPEED and CAMERA_MOVE_SPEED constants on lines 61 and 62.

 
