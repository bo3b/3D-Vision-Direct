# 3D-Vision-Direct
Small stereoscopic sample demonstrating how to use 3D Vision Direct Mode using DX11.  

I could not find any example code showing how to use Direct Mode, while just using the nvapi, so I developed this example.

All older examples use a rendertarget with an extra line for the stereo signature.  This example works
without requiring the stereo signature or any special setup, other than doubling the size of the backbuffer.
<br>
<br>

This is based upon the Microsoft DX11 Tutorial sample:   
https://code.msdn.microsoft.com/windowsdesktop/Direct3D-Tutorial-Win32-829979ef

Newest version:  
https://github.com/walbourn/directx-sdk-samples/tree/master/Direct3D11Tutorials
<br>
<br>

The piece used here is only Tutorial_07, which shows the full tutorial with a spinning cube, drawn using shaders and vertex buffer.

The Tutorial was modifed as little as possible, while adding the NVidia 3D Vision Direct Mode support.  
After initializing Direct Mode, the projection matrix is setup for stereo drawing, and then rendering is done twice, once for each eye.
