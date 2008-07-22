package uk.ac.ucl.chem.ccs.clinicalgui;
import java.awt.GridLayout;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;
import java.io.FileInputStream;
import java.io.IOException;
import java.util.Properties;
import uk.ac.ucl.chem.ccs.aheclient.res.AdvancedReservation;
import uk.ac.ucl.chem.ccs.clinicalgui.res.ResPanel; 
import uk.ac.ucl.chem.ccs.clinicalgui.res.ViewReservation;

import javax.swing.JMenu;
import javax.swing.JMenuBar;
import javax.swing.JMenuItem;
import javax.swing.JPanel;
import javax.swing.JSeparator;

import org.apache.commons.logging.Log;
import org.apache.commons.logging.LogFactory;
import org.apache.log4j.PropertyConfigurator;
import org.jdesktop.application.SingleFrameApplication;



/**
* This code was edited or generated using CloudGarden's Jigloo
* SWT/Swing GUI Builder, which is free for non-commercial
* use. If Jigloo is being used commercially (ie, by a corporation,
* company or business for any purpose whatever) then you
* should purchase a license for each developer using Jigloo.
* Please visit www.cloudgarden.com for details.
* Use of Jigloo implies acceptance of these licensing terms.
* A COMMERCIAL LICENSE HAS NOT BEEN PURCHASED FOR
* THIS MACHINE, SO JIGLOO OR THIS CODE CANNOT BE USED
* LEGALLY FOR ANY CORPORATE OR COMMERCIAL PURPOSE.
*/
/**
 * 
 */
public class ClinicalGuiClient extends SingleFrameApplication {
    private JPanel topPanel;
    private JMenuItem exit;
    private MainPanel mainPanel1;
    private JMenuItem configureAG;
    private JSeparator jSeparator1;
    private JMenuItem manage;
    private JMenuItem change;
    private JMenu jMenu1;
    private JMenuBar jMenuBar1;
    private static String propertiesFile;
    private JMenuItem agToolKitLaunch;
    private JMenu accessGridMenu;
	private static Log cat;

    public static Properties prop;
    public static AdvancedReservation reservation = null;
    public static int patientID = 0;
    public static String PATIENT_ID = "NONE";
    
	//@Override
    protected void startup() {
    	loadProperties();
    	{
	    	getMainFrame().setSize(800, 600);
    	}
    	{
    		jMenuBar1 = new JMenuBar();
    		getMainFrame().setJMenuBar(jMenuBar1);
    		{
    			jMenu1 = new JMenu();
    			jMenuBar1.add(jMenu1);
    			jMenu1.setName("jMenu1");
    			/*
    			{
    				change = new JMenuItem();
    				jMenu1.add(change);
    				change.setName("Change");
    				change.addActionListener(new ActionListener() {
						public void actionPerformed (ActionEvent evt) {
						    PatientIdDialog pd = new PatientIdDialog(ClinicalGuiClient.this.getMainFrame());
							pd.setModal(true);
							pd.setVisible(true);
						}
					});
    			}
    			*/
    			{
    				manage = new JMenuItem();
    				jMenu1.add(manage);
    				manage.setName("manage");
    			}
    			{
    				jSeparator1 = new JSeparator();
    				jMenu1.add(jSeparator1);
    			}
    			{
    				exit = new JMenuItem();
    				jMenu1.add(exit);
    				exit.setName("Exit");
    				exit.addActionListener(new ActionListener (){ 
						public void actionPerformed (ActionEvent e) {
							 closeApp();
						}
					});
    			}
    		}
    		{
    			accessGridMenu = new JMenu();
    			jMenuBar1.add(accessGridMenu);
    			accessGridMenu.setName("accessGridMenu");
    			{
    				agToolKitLaunch = new JMenuItem();
    				accessGridMenu.add(agToolKitLaunch);
    				agToolKitLaunch.setName("agToolKitLaunch");
    				agToolKitLaunch.setEnabled(false);
    			}
    			{
    				configureAG = new JMenuItem();
    				accessGridMenu.add(configureAG);
    				configureAG.setName("configureAG");
    				configureAG.setEnabled(false);
    			}
    		}
    	}
        topPanel = new JPanel();
        GridLayout topPanelLayout = new GridLayout(1, 1);
        topPanelLayout.setColumns(1);
        topPanelLayout.setHgap(5);
        topPanelLayout.setVgap(5);
        topPanel.setPreferredSize(new java.awt.Dimension(800, 600));
        topPanel.setLayout(topPanelLayout);
        {
        	mainPanel1 = new MainPanel();
        	topPanel.add(mainPanel1);
        }
        show(topPanel);
    }

    public static void main(String[] args) {
		try {
			PropertyConfigurator.configure(System.getProperty("log4j.configuration"));
			} catch (Exception e) {
				
			}
			cat = LogFactory.getLog(ClinicalGuiClient.class);
        launch(ClinicalGuiClient.class, args);
        //ErrorMessage ems = new ErrorMessage(ClinicalGuiClient.this.get), "testing...");
    }
    
    
    private static void loadProperties () {
    	String confprop = System.getProperty("uk.ac.ucl.chem.ccs.aheclient.conffile");
		
    	if (confprop != null && !confprop.equals("")) {
			propertiesFile = confprop;
			System.out.println("foo" + propertiesFile);
		} else {
			propertiesFile = "aheclient.properties";
		}
		
    	 prop = new Properties();
    	
	    try {
	        prop.load(new FileInputStream(propertiesFile));
	        cat.info("Opened properties file " + propertiesFile);
	    } catch (IOException e) {
	    	cat.error("Couldn't open properties file " + propertiesFile + "!");
			cat.error(e.getMessage());
			cat.debug(e.getStackTrace());
	    	cat.error("Configure client settings now");
	    	prop.setProperty("uk.ac.ucl.chem.ccs.aheclient.myproxy-server", "myproxy.grid-support.ac.uk");
	    	prop.setProperty("uk.ac.ucl.chem.ccs.aheclient.myproxy-dn", "/C=UK/O=eScience/OU=CLRC/L=DL/CN=host/myproxy.grid-support.ac.uk/E=a.j.richards@dl.ac.uk");
	    	prop.setProperty("uk.ac.ucl.chem.ccs.aheclient.myproxy-port", "7512");
	    	prop.setProperty("uk.ac.ucl.chem.ccs.aheclient.myproxy-lifetime", "7512");
	    	prop.setProperty("uk.ac.ucl.chem.ccs.aheclient.myproxy-un", "");
	    	prop.setProperty("uk.ac.ucl.chem.ccs.aheclient.myproxy-pw", "");
	    	prop.setProperty("uk.ac.ucl.chem.ccs.aheclient.ahedavuser", "");
	    	prop.setProperty("uk.ac.ucl.chem.ccs.aheclient.ahedavpasswd", "");
	    	prop.setProperty("uk.ac.ucl.chem.ccs.aheclient.ahedavserver", "");
	    	prop.setProperty("uk.ac.ucl.chem.ccs.aheclient.jobregepr", "");
	    	prop.setProperty("uk.ac.ucl.chem.ccs.aheclient.keystore", "aheclient.ks");
	    	prop.setProperty("uk.ac.ucl.chem.ccs.aheclient.jobfactoryepr", "");
	    	prop.setProperty("uk.ac.ucl.chem.ccs.aheclient.mpcheck", "false");
	    	prop.setProperty("uk.ac.ucl.chem.ccs.aheclient.mpwarn", "false");
	    }
    }
    
	private void closeApp() {
		System.exit(0);
	}

}
