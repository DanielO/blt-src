
How to build BLT on Windows.
============================

Windows 32-bit and 64-bit have been tested with Tcl/Tk 8.4-8.6.  You'll
need **cygwin** or **msys** set up to compile.  *I no longer support VC++,
Borland C, etc.*

The configure script determines if you are building for a 32-bit or 64-bit
system from the compiler used.

Prerequisites
=============

The only requirement for building BLT is for the Tcl/Tk headers and include
files to installed.  You can build Tcl/Tk yourself, or install them from a
distributed package. BLT does not require the **Tcl/Tk** C source files.

  **Tcl/Tk 8.4-8.6**		
    The headers and include files must be installed.  
	
The following libraries and include files are optional.  They are used in
specific BLT sub-packages.

  **expat**
    for blt::datatable and blt::tree XML parsers.
  **jpeg**			
    for picture image to read jpeg files.		  
  **png**		
    for picture image to read png files.		  
  **tiff**
    for picture image to read tiff files.		  
  **freetype**
    for drawing text on picture images.
  **libssh2**
    for sftp package.		  
  **sqlite**
    for blt::datatable.
  **libpostgres**
    for blt::datatable.
  **libmysqlclient**
    for blt::datatable.

Configure Options 
=================

 ================================== ========================================
 Configure Option                   Description                       
 ---------------------------------- ----------------------------------------
 **--enable-shared**	            Created shared libraries.  
 **--enable-stubs**		    Build stubbed version of BLT libraries.
 **--enable-symbols**		    Compile with debugging symbols.
 **--enable-xshm**		    Use X Shared Memory extension   
 **--with-blt=**\ *dir*             Install BLT scripts in *dir*       
 **--with-expatincdir=**\ *dir*     Find expat headers in *dir*  
 **--with-expatlibdir=**\ *dir*     Find expat libraries in *dir*  
 **--with-freetype2incdir=**\ *dir* Find freetype2 headers in *dir*  
 **--with-freetype2libdir=**\ *dir* Find freetype2 libraries in *dir*  
 **--with-gnu-ld**                  Use GNU linker.
 **--with-jpegincdir=**\ *dir*      Find JPEG headers in *dir*          
 **--with-jpeglibdir=**\ *dir*      Find JPEG libraries in *dir*        
 **--with-libssh2incdir=**\ *dir*   Find libssh2 headers in *dir*  
 **--with-libssh2libdir=**\ *dir*   Find libssh2 libraries in *dir*  
 **--with-mysqlincdir=**\ *dir*     Find mysql headers in *dir*  
 **--with-mysqllibdir=**\ *dir*     Find mysql libraries in *dir*  
 **--with-openssllibdir=**\ *dir*   Find openssl libraries in *dir*  
 **--with-pngincdir=**\ *dir*       Find PNG headers in *dir*           
 **--with-pnglibdir=**\ *dir*       Find PNG libraries in *dir*         
 **--with-pqincdir=**\ *dir*        Find Postgres headers in *dir*  
 **--with-pqlibdir=**\ *dir*        Find Postgres libraries in *dir*  
 **--with-sqliteincdir=**\ *dir*    Find sqlite headers in *dir*  
 **--with-sqlitelibdir=**\ *dir*    Find sqlite libraries in *dir*  
 **--with-tcl=**\ *dir*             Find tclConfig.sh in *dir*
 **--with-tclincdir=**\ *dir*       Find Tcl includes in *dir*         
 **--with-tcllibdir=**\ *dir*       Find Tcl libraries in *dir*         
 **--with-tiffincdir=**\ *dir*      Find TIFF headers in *dir*          
 **--with-tifflibdir=**\ *dir*      Find TIFF libraries in *dir*        
 **--with-tk=**\ *dir*              Find tkConfig.sh in *dir*
 **--with-tkincdir=**\ *dir*        Find Tk includes in *dir*           
 **--with-tklibdir=**\ *dir*        Find Tk libraries in *dir*          
 **--with-xftincdir=**\ *dir*       Find Xft headers in *dir*  
 **--with-xftlibdir=**\ *dir*       Find Xft libraries in *dir*  
 **--with-xpmincdir=**\ *dir*       Find XPM headers in *dir*           
 **--with-xpmlibdir=**\ \ *dir*     find XPM libraries in *dir*  
 **--with-xrandrincdir=**\ *dir*    Find Xrandr headers in *dir*
 **--with-xrandrlibdir=**\ *dir*    Find Xrandr libraries in *dir*  
 **--with-zlibdir=**\ *dir*         Find zlib libraries in *dir*        
 ================================== ========================================

1. Unpack the BLT tar file.

       tar -xvf blt.tgz 

   This will create a "blt" directory.

2. Create a build directory

       mkdir build
       cd build

3. Note For **cygwin**:  To compile a native windows version with the **cygwin**
   compiler you'll need to specify the compiler with the CC environment
   variable.  

   ::

	CC=i686-x64-ming32-gcc
	export CC

   Both Tcl and Tk must also be a native windows version (non-cygwin).

   You don't have do anything to compile a cygwin version.

3. Run "configure" to set what packages an options you want.

   ::

       ../configure --prefix=<where you want to install> \
          --exec-prefix=<where you want to install>

   Add --enable-stubs to build stub-ed versions.

   Example:
 
   ::

       ../configure --prefix=C:/tcltk \
          --exec-prefix=C:/tcltk
          --with-expatlibdir=$(libdir) \
          --with-pnglibdir=$(libdir) \
          --with-jpeglibdir=$(libdir) \
          --with-tifflibdir=$(libdir) \
          --with-freetype2libdir=$(libdir) \
          --with-libssh2libdir=$(libdir) \
          --with-libssh2incdir=$(incdir) \
          --with-openssllibdir=$(SSLDIR)/lib \
          --enable-shared \
          $(common_flags)


gcc mingw32 or mingw64 can create Windows executables and DLLs.

1.  If you are building Tk 8.5 with mingw32/64, you need to fix the 
    source code in win/winMain.c to add __MINGW32__ to the __CYGWIN__ 
    defines.

    sed -i 's/defined(__CYGWIN__)/defined(__CYGWIN__) || defined(MINGW32)' 
       win/winMain.c

2.  The bltwish demo is a statically built executable. It doesn't
    work with --enable-stubs.
