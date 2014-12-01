Project Setup - GLWallpaperService
==================================

Discussion Group:
http://groups.google.com/group/glwallpaperservice

Repository:
https://github.com/markfguerra/GLWallpaperService/

Intended Audience
-----------------
This document is for developers who want to improve GLWallpaperService. It shows you how to download the source code to to improve it (or break it) as you please using Eclipse.
If you're only interested in using the software to make an OpenGL Wallpaper, this isn't for you. Instead, look at readme.txt for info on how to get and install the Jar.


Install required software
-------------------------
To start, make sure install Eclipse the latest Android SDK are installed. Optionally, install git. Follow the instructions provided by those software projects to do so.
    Eclipse:     http://wiki.eclipse.org/FAQ_Where_do_I_get_and_install_Eclipse%3F
    Android SDK: http://developer.android.com/sdk/installing.html
	Git:         http://git-scm.com/

Also, if you plan on using git, we recommend you make an account on github.
	http://github.com/


Folder Setup
------------
We are going to make two folders in this document. One will contain the code itself, and another will contain the Eclipse workspace. If you prefer that they both be in the same folder, do so. As if I could stop you ;)

Go to where you're putting all this stuff.
    cd /path/to/your/folder

Make a folder for your workspace.
    mkdir workspace


Get the code
------------
Download the source code. You can do so using git or just a regular old download.

For a direct download, go here. Extract the zip into a sub folder:
    https://github.com/markfguerra/GLWallpaperService/zipball/master

If you use github, please fork this code from their web interface:
    https://github.com/markfguerra/GLWallpaperService

Then you want to make a local clone of your github fork. Github will provide you with your own Url. My clone command looks like this:
    git clone git@github.com:markfguerra/GLWallpaperService.git

A new folder will be created automatically, called GLWallpaperService/ containing your source code.

Visit this link if you need help using github:
    http://help.github.com/


Start Eclipse
-------------
Open Eclipse and choose the folder you just created for your workspace. When it finishes loading, click the Arrow to go to the workbench.


Set up the Android SDK
----------------------
To set up the Android SDK for this project:
In the menu, go to Eclipse->Preferences. Go to the Android section.
Give it the location of your Android SDK folder. Give it a minute to figure things out.
Select Android 2.1 (Api level 7) then click Ok.


Import the code into the workspace
----------------------------------
In the Eclipse menu, choose File->New->"Android Project"
Choose "Create new project from existing source"
For the location click browse and select the "GLWallpaperService" folder. To be clear, this is a sub-folder of the folder that contains the LICENSE file.
Select "Android 2.1-update1" as your build target. This is API Level 7.
For the Project Name, type "GLWallpaperService"
Repeat these steps for the "GLWallpaperTest" folder. Use the project name "GLWallpaperTest"

The code will now be in your workspace.


Resolving build errors
----------------------
Most people should have a working project at this point. However, some folks encounter build errors when they first import the code into their workspace. There are a couple of tricks to help you fix them.
First and foremost, make sure you have the latest versions of the Android SDK and Eclipse plugin. This is important, because older versions of the developer tools handle Library Projects differently, so if you have an old version this may cause problems.
You may have to fix the Project Properties. Right-click on the "GLWallpaperService" project in the Package Explorer. Select "Android Tools"->"Fix Project Properties".
Also fix the project properties for "GLWallpaperTest" the same way.
In the menu, click on Project->Clean to clean all projects.
In Package Explorer, right-click on the GLWallpaperService project and click on "Refresh". Do the same for GLWallpaperTest.
You may need to click on Project->Clean to clean all projects again after the refresh.
Your errors should go away at this point. If not, try using a programming Q&A site such as stackoverflow.com and also search Google.


To Run
------
Save your work. By default, saving will also compile your code in Eclipse.

Plug in your Android Device to your computer, if you have one. If you don't plug in an Android device, the emulator will launch when you run the code. You may need create an Android Emulator if you havn't done so already.

In the Menu, choose Run->"Run Configurations".
Choose "Android Application"
Click on the "New" button. This is the button with the plus sign on it.
The name of the run configuration is New_configuration. Change it if you like.
Click "Browse..." to choose a project. Select GLWallpaperTest. This is a sample wallpaper that comes with the code.
Click Apply. Click Run.
The wallpaper will now install on your device or emulator. Use the regular Android live wallpaper picker to select "GL Wallpaper Test Project". You should see a 3D rotating cube.

Now that your run configuration is set up, you can run New_configuration any time by using the Toolbar icon.


Creating GLWallpaperService.jar
-------------------------------
Do the following to create the JAR file, which is convenient for use in projects.
In the Eclipse menu, click File -> Export
Choose "JAR File"
On the "JAR Export" screen, choose only GLWallpaperService.java. You do not need to include other files.
Make note of where the JAR will be saved and click Finish


Contribute your code
--------------------
If you do something cool with this software, we would love it if you would share your changes with us. Even if your changes still need some polishing. Don't worry, we're friendly :)

If you are using git and github, push your changes to your fork and send us a pull request. If you're not using those fancy things, we want to hear from you anyway. Send us a message and we'll work something out.

Thanks for your interest in this project. Good luck coding!

