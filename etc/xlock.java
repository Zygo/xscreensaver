// charles vidal 28.08.97 <cvidal@newlog.com>
// the main file , with the main class 
// to compile javac xlock.java
// to launch java xlock
// in a browser you could show the GUI but , not launch the program

import java.awt.*;
import xlockFrame;

class xlock extends java.applet.Applet
{
int isapp=0;
public void init(){
Frame theAppWindow = new Frame("FrameTest");
xlockFrame theApplet = new xlockFrame();
theApplet.init();
theApplet.start();
theAppWindow.add("Center",theApplet);
theAppWindow.resize(350,400);
theAppWindow.show();
}
public static  void main(String args[]) {
xlock xpj = new xlock();
xpj.init();
xpj.start();
}

}
