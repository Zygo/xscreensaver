// charles vidal <vidalc@club-internet.fr>
// the GUI definition and the handler event
import java.applet.*;
import java.awt.*;
import java.lang.Runtime;

public class xlockFrame extends Applet {

public static final int nbcoption=9;
public static final int nbbooleanopt=14;
Button blaunch;
Button blaunchinw;
Button bquit;
Choice coptions;
TextField foroptions;
 List lst ;
int	currentOption=0;

MyFrameError mfe;

boolean isinapp=false;

// Array of option name
String[] nomOption={"Name program",
"File",
"Message Password",
"Message Valid",
"Message Invalid",
"Prompt",
"Fonts",
"Geometry","Display"};

String[] valueOption={"","","","","","","","",""};
String[] cmdlineOption={"-program","-messagefile","-password","-validate","-invalid","","-font","-geometry","-display"};

// Array of option boolean name
String[] booleanOption={
"-mono ",
"-nolock ",
"-remote ",
"-allowroot ",
"-enablesaver ",
"-allowaccess ",
"-grabmouse ",
"-echokeys ",
"-usefirst ",
"-verbose ",
"-inroot ",
"-timeelapsed ",
"-install ",
"-use3d "};
Checkbox bopt[]=new Checkbox[15];
 public void init()
        {
 Frame theAppWindow = new Frame("xlockFrame");
 Panel Panel1 = new Panel();
 Panel Panel2 = new Panel();
 Panel Panel3 = new Panel();
 setLayout(new BorderLayout(10,10));

 lst = new List();

coptions=new Choice();
for (int i=0;i<nbcoption;i++)
	coptions.addItem(nomOption[i]);

Panel3.add(coptions);

foroptions = new TextField(20);
Panel3.add(foroptions);

add("North", Panel3);

add("Center",lst);
$%LISTJAVAlst.addItem("bomb");
lst.addItem("random");
lst.select(0);

add("East", Panel1);
Panel1.setLayout(new GridLayout(15,1));
for (int i=0;i<nbbooleanopt;i++)
	{ bopt[i]=new Checkbox(booleanOption[i],null,false);
	Panel1.add(bopt[i]);
	}

add("South", Panel2);
blaunch=new Button("Launch");
blaunchinw=new Button("Launch in window");
bquit=new Button("Quit");
Panel2.add(blaunch);
Panel2.add(blaunchinw);
Panel2.add(bquit);
}
public String getBooleanOption()
{
String result="";
for (int i=0;i<nbbooleanopt;i++)
	{if (bopt[i].getState()) result=result.concat(booleanOption[i]);
	}
	return (result);
}
public boolean action (Event evt, Object arg)
{

Runtime r=Runtime.getRuntime();


if (isinapp)
	mfe=new MyFrameError("An error occured , You can't launch xlock");
	else
	mfe=new MyFrameError("An error occured , You can't launch xlock");
	//mfe=new MyFrameError("You can't launch by a Browser");

mfe.resize(350,150);

if (evt.target == blaunch || evt.target == blaunchinw)
	{
	String label= (String) arg;
	String cmdlinexlock="xlock ";
	if (evt.target == blaunchinw ) cmdlinexlock=cmdlinexlock.concat("-inwindow ");
	for (int i=0;i<nbcoption;i++)
		{
		if (!valueOption[i].equals(""))
			{
			cmdlinexlock=cmdlinexlock.concat(cmdlineOption[i]+" "+valueOption[i]+" ");
			}
		}
	cmdlinexlock=cmdlinexlock.concat(getBooleanOption());
	cmdlinexlock=cmdlinexlock.concat(" -mode ");
	cmdlinexlock=cmdlinexlock.concat(lst.getSelectedItem());
	try {
  System.out.println(cmdlinexlock);
	r.getRuntime().exec(cmdlinexlock); }
			catch ( Exception e )
				{mfe.show();}
	return true;}
else
	if (evt.target == coptions)
	{
	String label= (String) arg;
	valueOption[currentOption]=foroptions.getText();
	for (int i=0;i<nbcoption;i++)
		{
		if (nomOption[i].equals(label))
			{foroptions.setText(valueOption[i]);
			currentOption=i;
			}
		}
	}
else
	if (evt.target == bquit) {System.exit(0);}
	else if (evt.target instanceof Checkbox)
	{
	return true;
	}
else
	if (evt.target == foroptions)
	{
	String label= (String) arg;
	valueOption[currentOption]=label;
	return true;
	}
	return false;
  }

public void SetInAppl()
{
isinapp=true;
}

}
class MyFrameError extends Frame {
Label l;
Button b_ok;
MyFrameError (String erreur) {

	setLayout(new BorderLayout());
	l=new Label(erreur,Label.CENTER);
	add("Center",l);
	b_ok=new Button("Ok");
	add("South",b_ok);
	setTitle(erreur);
	setCursor(HAND_CURSOR);
	}
public boolean action (Event evt,Object arj){
	if (evt.target instanceof Button)
		{
		this.hide();
		return (true);
		}
		return (false);
}
}
