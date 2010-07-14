using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.IO.Ports;
using System.Threading;
using Microsoft.VisualBasic;
using System.IO;
using System.Runtime.InteropServices;
using System.Diagnostics;

namespace testANT_receiver
{



    public partial class frmMain : Form
    {

        //Devices combo box items (cbDevice)
        // 0 = Garmin device
        // 1 = Sparkfun device
        

        //Serial port used to connect to SparkFun device
        SerialPort port;
        //Flag to terminate the serial connection (to prevent thread deadlocks and hanging)
        bool terminateSerial = false;
        //Message buffers used to communicate with Garmin ANT+ device
        static byte[] strResponseBuffer;
        static byte[] strChannelEventBuffer;
        byte[] NET_EventBuffer;
        byte[] NET_ResponseBuffer;
        //Callback functions to communicate with Garmin ANT+ device
        ANT_DLL.ANT_ResponseFunctionCallback ResponseCallback;
        ANT_DLL.ANT_ChannelEventFunctionCallback ChannelEventCallback;
        IntPtr ptr_ResponseCallback;
        IntPtr ptr_EventCallback;
        IntPtr ptr_ResponseBuffer;
        IntPtr ptr_EventBuffer;

        //Message buffer to communicate with SparkFun device
        List<byte> bbuffer;

        //Some variables to handle the scripts
        List<string> messages;  //messages to display during script
        List<int> stage_lengths;    //duration of script stages
        List<int> recording;    //log HR data during script stage (1=yes, 2=no)
        List<Color> bg_colours;     //message BG colors during scripts stages
        int stage;  //The current stage number
        int stage_time;    //Length of time on current stage
        bool run_analysis;  //Determine if output data will be supplied to an analysis program after script completed
        string analysis_command;    //Executable command to program that will perform analysis

        //Determine if HR data is currently being logged
        bool is_recording;

        //Current HR and RR tracking values/buffers
        int hr_counter = -1;    //Garmin watch sends out a counter each time a new HR data packet is generated
        int rr_time = -1;       //RR-intervals are sent as an increasing timer
        List<int> rr_intervals; //list of RR intervals for this logging session
        DateTime start_time;    //time this logging session commenced
        bool device_open = false;   //connection currently open to ANT transmitter


        /// <summary>
        /// Contains constants to for ANT messages for communicating with SparkFun device
        /// </summary>
        enum ANT_msg : byte
        {
            _0xA4_MESG_TX_SYNC = (0xA4),
            _0x00_MESG_INVALID_ID = (0x00),
            _0x01_MESG_EVENT_ID = (0x01),
            _0x3E_MESG_VERSION_ID = (0x3E),  // protocol library version
            _0x40_MESG_RESPONSE_EVENT_ID = (0x40),
            _0x41_MESG_UNASSIGN_CHANNEL_ID = (0x41),
            _0x42_MESG_ASSIGN_CHANNEL_ID = (0x42),
            _0x43_MESG_CHANNEL_MESG_PERIOD_ID = (0x43),
            _0x44_MESG_CHANNEL_SEARCH_TIMEOUT_ID = (0x44),
            _0x45_MESG_CHANNEL_RADIO_FREQ_ID = (0x45),
            _0x46_MESG_NETWORK_KEY_ID = (0x46),
            _0x47_MESG_RADIO_TX_POWER_ID = (0x47),
            _0x48_MESG_RADIO_CW_MODE_ID = (0x48),
            _0x49_MESG_SEARCH_WAVEFORM_ID = (0x49),
            _0x4A_MESG_SYSTEM_RESET_ID = (0x4A),
            _0x4B_MESG_OPEN_CHANNEL_ID = (0x4B),
            _0x4C_MESG_CLOSE_CHANNEL_ID = (0x4C),
            _0x4D_MESG_REQUEST_ID = (0x4D),
            _0x4E_MESG_BROADCAST_DATA_ID = (0x4E),
            _0x4F_MESG_ACKNOWLEDGED_DATA_ID = (0x4F),
            _0x50_MESG_BURST_DATA_ID = (0x50),
            _0x51_MESG_CHANNEL_ID_ID = (0x51),
            _0x52_MESG_CHANNEL_STATUS_ID = (0x52),
            _0x53_MESG_RADIO_CW_INIT_ID = (0x53),
            _0x54_MESG_CAPABILITIES_ID = (0x54),
            _0x56_MESG_NVM_DATA_ID = (0x56),
            _0x57_MESG_NVM_CMD_ID = (0x57),
            _0x58_MESG_NVM_STRING_ID = (0x58),
            _0x59_MESG_ID_LIST_ADD_ID = (0x59),
            _0x5A_MESG_ID_LIST_CONFIG_ID = (0x5A),
            _0x5B_MESG_OPEN_RX_SCAN_ID = (0x5B),
            _0x5C_MESG_EXT_CHANNEL_RADIO_FREQ_ID = (0x5C),
            _0x5D_MESG_EXT_BROADCAST_DATA_ID = (0x5D),
            _0x5E_MESG_EXT_ACKNOWLEDGED_DATA_ID = (0x5E),
            _0x5F_MESG_EXT_BURST_DATA_ID = (0x5F),
            _0x60_MESG_CHANNEL_RADIO_TX_POWER_ID = (0x60),
            _0x61_MESG_GET_SERIAL_NUM_ID = (0x61),
            _0x62_MESG_GET_TEMP_CAL_ID = (0x62),
            _0x63_MESG_SET_LP_SEARCH_TIMEOUT_ID = (0x63),
            _0x64_MESG_SET_TX_SEARCH_ON_NEXT_ID = (0x64),
            _0x65_MESG_SERIAL_NUM_SET_CHANNEL_ID_ID = (0x65),
            _0x66_MESG_RX_EXT_MESGS_ENABLE_ID = (0x66),
            _0x67_MESG_RADIO_CONFIG_ALWAYS_ID = (0x67),
            _0x68_MESG_ENABLE_LED_FLASH_ID = (0x68),
            _0x6A_MESG_AGC_CONFIG_ID = (0x6A),
            _0xA0_MESG_READ_SEGA_ID = (0xA0),
            _0xA1_MESG_SEGA_CMD_ID = (0xA1),
            _0xA2_MESG_SEGA_DATA_ID = (0xA2),
            _0xA3_MESG_SEGA_ERASE_ID = (0xA3),
            _0xA4_MESG_SEGA_WRITE_ID = (0xA4),
            _0xA6_MESG_SEGA_LOCK_ID = (0xA6),
            _0xA7_MESG_FUSECHECK_ID = (0xA7),
            _0xA8_MESG_UARTREG_ID = (0xA8),
            _0xA9_MESG_MAN_TEMP_ID = (0xA9),
            _0xAA_MESG_BIST_ID = (0xAA),
            _0xAB_MESG_SELFERASE_ID = (0xAB),
            _0xAC_MESG_SET_MFG_BITS_ID = (0xAC),
            _0xAD_MESG_UNLOCK_INTERFACE_ID = (0xAD),
            _0xB0_MESG_IO_STATE_ID = (0xB0),
            _0xB1_MESG_CFG_STATE_ID = (0xB1),
            _0xC0_MESG_RSSI_ID = (0xC0),
            _0xC1_MESG_RSSI_BROADCAST_DATA_ID = (0xC1),
            _0xC2_MESG_RSSI_ACKNOWLEDGED_DATA_ID = (0xC2),
            _0xC3_MESG_RSSI_BURST_DATA_ID = (0xC3),
            _0xC4_MESG_RSSI_SEARCH_THRESHOLD_ID = (0xC4),
            _0xD0_MESG_BTH_BROADCAST_DATA_ID = (0xD0),
            _0xD1_MESG_BTH_ACKNOWLEDGED_DATA_ID = (0xD1),
            _0xD2_MESG_BTH_BURST_DATA_ID = (0xD2),
            _0xD3_MESG_BTH_EXT_BROADCAST_DATA_ID = (0xD3),
            _0xD4_MESG_BTH_EXT_ACKNOWLEDGED_DATA_ID = (0xD4),
            _0xD5_MESG_BTH_EXT_BURST_DATA_ID = (0xD5),

        }


        public frmMain()
        {
            InitializeComponent(); 
        }


        /// <summary>
        /// Parses data received form the Sparkfun ANT wireless device, and looks for data messages. When a complete
        /// message has been received it is passed on to ProcessMessage for further processing
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        void port_DataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            //Close the serial port if requested (call port.Close from the main UI thread
            //can sometimes cause deadlock issues...
            if (terminateSerial)
            {
                port.Close();
                terminateSerial = false;
                return;
            }


            //There are several cases to handle
            //1) We are not in a message, so wait until the 0xA4 byte is received
            //2) The byte received is 0xA4, then check the next byte which will indicate the
            //      number of data bytes in this message (if this is > 9 then discard)
            //3) Wait until at least the correct number of bytes are available in the queue
            //4) Look at the checksum and check it matches
            //5) If all good, send the whole messages on for further processing, and goto 1) to scan for more messages if available

            while (port.BytesToRead > 0)
            {
                bbuffer.Add((byte)port.ReadByte());
            }

            //ANT message format is:
            //SYNC, MSG_LENGTH, MSG_ID, DATA1, ...., DATAN, CHECKSUM
            //so entire message is num. data bytes + 4 (SYNC, MSG_LENGTH, MSG_ID, CHECKSUM)
            while (true)    //keep scanning through messages until function returns
            {
                //If there is no data in the receive buffer, then do nothing
                if (bbuffer.Count == 0) return;

                //Scan through the receive buffer until 0xA4 is found
                int list_index = 0;
                while (bbuffer[list_index] != (byte)ANT_msg._0xA4_MESG_TX_SYNC)
                {
                    list_index++;
                    if (list_index == bbuffer.Count) return;
                }
                //Discard any characters up to sync byte
                bbuffer.RemoveRange(0, list_index);

                //See if the MSG_LENGTH has arrived yet, if not then exit and wait until next time data is received
                if (bbuffer.Count < 2) return;
                byte msg_length = bbuffer[1];

                //MSG_LENGTH can only be 1 to 9, so if outside those ranges discard the message
                if (msg_length > 9 || msg_length < 1)
                {
                    bbuffer.RemoveAt(0);  //get rid of sync character and continue search
                    return;
                }

                //See if entire message has arrived yet
                if (bbuffer.Count < msg_length + 4) return;

                //copy message to array
                byte[] new_msg = new byte[msg_length + 4];
                bbuffer.CopyTo(0, new_msg, 0, msg_length+4);
                byte checksum = 0;
                //Compute and verify checksum of message
                for (int i = 0; i < msg_length + 3; i++)
                {
                    checksum ^= new_msg[i];
                }
                if (checksum != new_msg[new_msg.Length - 1])
                {
                    //Checksum indicates message not valid, so discard first sync byte
                    //and data will be re-processed on next function call
                    bbuffer.RemoveAt(0);
                    return;
                }
                //Checksum valid, clear from queue and pass data on for further processing
                bbuffer.RemoveRange(0, msg_length + 4);
                //ProcessMessage(new_msg);

                Invoke(new EventHandler(delegate { ProcessMessage(new_msg); }));   //use Invoke as serial timer is on a different thread
            }
        }


        /// <summary>
        /// Processes a full message received from the HR device, and computes HR, RR. Also displays device ID if 
        /// 0x51 response message received
        /// </summary>
        /// <param name="msg">Byte array of received message</param>
        void ProcessMessage(byte[] msg)
        {

            //Write the raw message
            string strings = String.Empty;
            for (int i = 0; i < msg.Length; i++)
            //for (int i = 0; i < 12; i++)
            {
                strings = strings + ' ' + msg[i].ToString("X2");
            }
            strings = strings + "\r\n";

            txtOutput.Text = strings;

            //heart rate data messages are sent as:
            //SYNC, MSG_LENGTH (=9), MSG_ID (=0x4E), DEVICE/MODEL DATA (=5 bytes), RR_TIMER(=2bytes), HR(=1 byte), CHECKSUM
            //0 - SYNC
            //1 - MSG_LENGTH 
            //2 - MSG_ID (0x4E)
            //3 - DEVICE/MODEL ID
            //4 - DEVICE/MODEL ID
            //5 - DEVICE/MODEL ID
            //6 - DEVICE/MODEL ID
            //7 - DEVICE/MODEL ID
            //8 - RR_TIMER
            //9 - RR_TIMER
            //10 - DATA COUNTER
            //11 - HR
            //12 - CHECKSUM

            //device ID data messages are sent as:
            //0 - SYNC
            //1 - MSG DATA LENGTH 
            //2 - MSG_ID (0x51)
            //3 - CHANNEL NO
            //4 - DEVICE ID
            //5 - DEVICE ID (MULTIPLY BY 256)
            //6 - DEVICE TYPE ID
            //7 - TRANSMISSION TYPE


            //Determine what kind of message has been received
            switch (msg[2])
            {
                case 0x4e:
                    //Heart rate data message
                    //See if this is a new HR message, or a repeat of the last message
                    if (hr_counter != -1)
                    {
                        if (hr_counter == msg[10]) break;
                    }
                    hr_counter = msg[10];
                    txtHR.Text = msg[11].ToString() + " BPM";
                    int rr_now = msg[9] * 256 + msg[8];
                    if (rr_time == -1)
                    {
                        rr_time = rr_now;
                        break;
                    }
                    int rr = rr_now - rr_time;
                    if (rr < 0)
                    {
                        rr = 65536 + rr;
                    }
                    rr_time = rr_now;
                    txtRR.Text = rr.ToString("0.0") + " ms";
                    if (is_recording)
                    {
                        rr_intervals.Add(rr);
                    }
                    //Write the processed message
                    string msg_str = hr_counter.ToString() + ", " + msg[11].ToString() + "BPM, " + rr.ToString() + "ms\r\n";


                    break;
                case 0x51:
                    //Device ID message
                    int device_id = msg[5] * 256 + msg[4];
                    MessageBox.Show("Connected to device ID: " + device_id.ToString());
                    break;
                default:
                    break;
            }


        }

        /// <summary>
        /// Load the form and initialise storage variables/etc.
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void frmMain_Load(object sender, EventArgs e)
        {
            //Populate list of COM ports available
            string[] portNames = SerialPort.GetPortNames();
            for (int i=0; i<portNames.Length; i++) {
                cbCOMPort.Items.Add(portNames[i]);
            }
            if (cbCOMPort.Items.Count > 0)
            {
                cbCOMPort.SelectedIndex = cbCOMPort.Items.Count-1;

            }

            //Initialise memory buffer for SparkFun device
            bbuffer = new List<byte>();

            //Initialise buffers to communicate with Garmin ANT+ device
            NET_EventBuffer = new byte[32];
            NET_ResponseBuffer = new byte[32];

            //Initialise callback functions for communicating with Garmin deivce
            ResponseCallback = new ANT_DLL.ANT_ResponseFunctionCallback(ResponseFunction);
            ChannelEventCallback = new ANT_DLL.ANT_ChannelEventFunctionCallback(ChannelEventFunction);
            //Allocate handles to prevent Garbage collector collecting functions passed to unmanaged code
            ptr_EventBuffer = Marshal.AllocHGlobal(sizeof(byte) * 32);
            ptr_ResponseBuffer = Marshal.AllocHGlobal(sizeof(byte) * 32);
            ptr_EventCallback = Marshal.GetFunctionPointerForDelegate(ChannelEventCallback);
            ptr_ResponseCallback = Marshal.GetFunctionPointerForDelegate(ResponseCallback);

            //Create a folder to save program settings and data output
            string save_path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "HRV_tracker");
            if (!Directory.Exists(save_path))
            {
                Directory.CreateDirectory(save_path);
                MessageBox.Show("Welcome to HRV tracker. Please view the Help documentation for assistance in getting started with this program.\r\n\r\nA folder has been created to store recorded data and program settings at: " + save_path, "HRV tracker");
            }


            //Simple settings text file to select the default device and baud rate
            string settings_path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "HRV_tracker");
            string filename = Path.Combine(settings_path, "settings.txt");
            if (File.Exists(filename)) {
                StreamReader sr = new StreamReader(filename);
                string line;
                line = sr.ReadLine();
                cbDevice.SelectedIndex = int.Parse(line);
                line = sr.ReadLine();
                txtBaudRate.Text = line;
                line = sr.ReadLine();
                txtDeviceID.Text = line;
                sr.Close();
            }

            //If a script file was supplied on the command line, load and run the script
            string[] args = Environment.GetCommandLineArgs();
            if (args.Length > 1)
            {
                txtScriptFilename.Text = args[1];
                StartScript(txtScriptFilename.Text);
            }

        }

        /// <summary>
        /// Sends a message via UART to the ANT wireless device. 
        /// </summary>
        /// <param name="msg">Array of bytes to send with message, in the format [MSG_ID, DATA1....DATAN] and 
        /// ANT_send will prefix with the SYNC and MSG_LENGTH bytes, and suffix with the checksum</param>
        private void ANT_send(byte[] msg)
        {
            byte[] send_msg = new byte[msg.Length + 3];
            send_msg[0] = (byte)ANT_msg._0xA4_MESG_TX_SYNC;   //Sync byte
            send_msg[1] = (byte)(msg.Length - 1);       //Num data bytes in message
            Array.Copy(msg, 0, send_msg, 2, msg.Length);   //Copy the message and data
            for (int i = 0; i < send_msg.Length - 1; i++)
            {
                send_msg[send_msg.Length-1] ^= send_msg[i];    //Create checksum of XOR all previous bytes in message
            }
            //Write the message to the UART connection
            if (port.IsOpen)
            {
                port.Write(send_msg, 0, send_msg.Length);
            }
        }



        /// <summary>
        /// Loads and parses a script file, opens connection to HRM and starts the scipt at first stage.
        /// </summary>
        /// <param name="script_filename">Filename to the script file</param>
        private void StartScript(string script_filename)
        {
            tmrStage.Stop();
            is_recording = false;
            messages = new List<string>();
            stage_lengths = new List<int>();
            recording = new List<int>();
            bg_colours = new List<Color>();
            run_analysis = false;
            analysis_command = String.Empty;

            if (!File.Exists(script_filename))
            {
                MessageBox.Show("Please select a valid script filename.");
                return;
            }

            StreamReader sr = new StreamReader(script_filename);
            string curr_line;
            string[] commands;
            while (!sr.EndOfStream)
            {
                curr_line = sr.ReadLine();
                if (!curr_line.StartsWith("//") && !curr_line.StartsWith("[")) {
                    commands = curr_line.Split(',');
                    if (commands.Length != 6)
                    {
                        MessageBox.Show("There was an error reading the script file.");
                        sr.Close();
                        return;
                    }
                    try
                    {
                        messages.Add(commands[0]);
                        stage_lengths.Add(int.Parse(commands[1]));
                        recording.Add(int.Parse(commands[2]));
                        bg_colours.Add(Color.FromArgb(int.Parse(commands[3]), int.Parse(commands[4]), int.Parse(commands[5])));
                    }
                    catch
                    {
                        MessageBox.Show("There was an error reading the script file.");
                        sr.Close();
                        return;
                    }
                }
                else if (curr_line.StartsWith("[Analysis]")) {
                    commands = curr_line.Split(',');
                    if (commands.Length != 2)
                    {
                        MessageBox.Show("There was an error reading the [Analysis] command in the script file.");
                        sr.Close();
                        return;
                    }
                    analysis_command = commands[1];
                    run_analysis = true;
                }
            }
            sr.Close();
            //Script is parsed, now begin execution
            //Connect to HR device (and reset if already connected...)
            if (OpenHRM() == false)
            {
                return;
            }

            //Set stage num and time to start
            stage = 0;
            stage_time = 0;
            if (recording[0] == 1)
            {
                StartRecording();
            }
            //Set up time to start
            lblMessage.Text = "Stage " + (stage+1).ToString() + "/" + stage_lengths.Count.ToString() + ": " + messages[stage];
            lblMessage.BackColor = bg_colours[stage];
            btnStartRecording.Enabled = false;
            btnStopRecording.Enabled = false;
            btnStartScript.Enabled = true;
            tmrStage.Start();

        }

        /// <summary>
        /// Called every 1 second when script is running to advance stage time, and determine if a new script
        /// stage has been reached
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void tmrStage_Tick(object sender, EventArgs e)
        {
            stage_time++;
            if (stage_time > stage_lengths[stage])
            {
                //Time for a new stage
                stage++;
                stage_time = 0;

                //Play a sound to indicate the new stage is here
                System.Media.SystemSounds.Beep.Play();

                if (stage == messages.Count)
                {
                    //End of script
                    string filename;
                    filename = StopRecording();
                    tmrStage.Stop();
                    CloseHRM();
                    lblRecording.Text = "Recording stopped.";
                    lblMessage.Text = "Script complete.";
                    btnStartRecording.Enabled = true;
                    btnStopRecording.Enabled = false;
                    btnStartScript.Enabled = true;

                    //if an analysis program was supplied, send the data onwards...
                    if (run_analysis)
                    {
                        Process analysis_program = new Process();
                        
                        analysis_program.StartInfo.FileName = analysis_command.Trim();
                        analysis_program.StartInfo.Arguments = "\"" + filename + "\"";
                        try
                        {
                            analysis_program.Start();
                        }
                        catch (Exception ex)
                        {
                            MessageBox.Show("Error starting analysis program: " + ex.Message);
                        }
                    }

                    return;
                }

                if (recording[stage] == 1)
                {
                    StartRecording();
                }
                else
                {
                    StopRecording();
                }
                lblMessage.Text = "Stage " + (stage + 1).ToString() + "/" + stage_lengths.Count.ToString() + ": " + messages[stage];
                lblMessage.BackColor = bg_colours[stage];

            }
            
            lblTime.Text = "Stage time: " + TimeSpan.FromSeconds(stage_time).ToString() + "/" + TimeSpan.FromSeconds(stage_lengths[stage]).ToString();
        }



        /// <summary>
        /// Clears recording buffers and begins logging data
        /// </summary>
        private void StartRecording()
        {
           if (is_recording)
           {
               return;
           }
            rr_intervals = new List<int>();
            start_time = DateTime.Now;
            is_recording = true;
            lblRecording.Text = "Recording...";
        }

        /// <summary>
        /// Sends a message to query the device ID on current channel. A message box is displayed in ProcessMessage if
        /// a response is received
        /// </summary>
        private void Query_ID()
        {
            if (!device_open)
            {
                MessageBox.Show("Not currently connected to a device. Start recodring to connect to a device and then query device ID.");
                return;
            }
            //Perform commands to query the device ID
            if (cbDevice.SelectedIndex == 0)
            {
                //Query Garmin ANT+
                ANT_DLL.ANT_RequestMessage(0, 0x51);
            }
            else
            {
                //Query SparkFun device
                ANT_send(new byte[] { (byte)ANT_msg._0x4D_MESG_REQUEST_ID, 0x0, 0x51 });
                Thread.Sleep(50);
            }

        }

        /// <summary>
        /// Saves the HRV data to an HRM file with a filename based on time/date, and stops recording. Returns the 
        /// filename of the created file.
        /// </summary>
        private string StopRecording()
        {
            bool in_script = false;

            if (is_recording == false)
            {
                return String.Empty;
            }
            else
            {
                if (tmrStage.Enabled)
                {
                    tmrStage.Enabled = false;
                    in_script = true;
                }
                //save data
                lblMessage.Text = "Saving data...";
                Application.DoEvents(); //Update the screen

                string save_path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "HRV_tracker");
                if (!Directory.Exists(save_path))
                {
                    Directory.CreateDirectory(save_path);
                }



                string fname = DateTime.Now.ToString("yyyy-MM-dd_HHmm") + ".hrm";
                string filename = Path.Combine(save_path, fname);

                StreamWriter sr = new StreamWriter(filename);
                sr.WriteLine("[Params]");
                sr.WriteLine("Date=" + start_time.ToString("yyyyMMdd"));
                sr.WriteLine("StartTime="+start_time.ToString("HH:mm:ss"));
                int total_HR = 0;
                foreach (int i in rr_intervals)
                {
                    total_HR += i;
                }
                TimeSpan ts = TimeSpan.FromMilliseconds(total_HR);
                sr.WriteLine("Length=" + ts.Hours.ToString("D2") + ":" + ts.Minutes.ToString("D2") + ":" + ts.Seconds.ToString("F01"));
                sr.WriteLine("Interval=238");
                sr.WriteLine();
                sr.WriteLine();
                sr.WriteLine("[HRData]");
                foreach (int i in rr_intervals)
                {
                    sr.WriteLine(i.ToString());
                }
                sr.Close();
                lblMessage.Text = "Saving data...done";
                if (in_script)
                {
                    tmrStage.Enabled = true;
                }
                is_recording = false;
                lblRecording.Text = "Recording stopped.";
                return filename;
            }

        }

        /// <summary>
        /// Closes connection to HRM
        /// </summary>
        private void CloseHRM()
        {
            //Perform commands to close the device
            if (cbDevice.SelectedIndex == 0)
            {
                CloseGarmin();
            }
            else
            {
                CloseSparkfun();
            }
            device_open = false;
            tsDeviceSetup.Enabled = true;
            txtHR.Text = "--- BPM";
            txtRR.Text = "--- ms";
        }


        /// <summary>
        /// Opens a connection to HRM device
        /// </summary>
        private bool OpenHRM()
        {           
            //reset device if already open...
            if (device_open)
            {
                CloseHRM();
            }

            //Check a valid HR device has been selected
            if (cbDevice.SelectedIndex == -1)
            {
                MessageBox.Show("Please select an HR monitor device.");
                return false;
            }

            //Check a valid device ID has been entered
            try
            {
                ushort.Parse(txtDeviceID.Text);
            }
            catch
            {
                MessageBox.Show("Please enter a valid device ID, or '0' to connect to any device.");
                return false;
            }


            //Perform commands to open the device
            if (cbDevice.SelectedIndex == 0)
            {
                if (OpenGarmin() == false)
                {
                    return false;
                }
            }
            else
            {
                if (OpenSparkfun() == false)
                {
                    return false;
                }
            }
            device_open = true;
            tsDeviceSetup.Enabled = false;
            hr_counter = -1;
            rr_time = -1;
            return true;
        }

        /// <summary>
        /// Callback supplied to ANT_DLL that is executed whenever a response to a
        /// request is received (eg: query the device ID)
        /// </summary>
        /// <param name="ucChannel"></param>
        /// <param name="ucResponseMessageID"></param>
        /// <returns></returns>
        public bool ResponseFunction(byte ucChannel, byte ucResponseMessageID)
        {

            Marshal.Copy(ptr_ResponseBuffer, NET_ResponseBuffer, 3, 10);
            NET_ResponseBuffer[2] = ucResponseMessageID;

            BeginInvoke(new EventHandler(delegate { ProcessMessage(NET_ResponseBuffer); }));
            return true;
        }

        /// <summary>
        /// Callback supplied to ANT_DLL that is executed whenever a channel event occurs
        /// </summary>
        /// <param name="ucChannel"></param>
        /// <param name="ucEvent"></param>
        /// <returns></returns>
        public bool ChannelEventFunction(byte ucChannel, byte ucEvent)
        {
            //Invoking the process message function might be taking too long, causing a backlog of
            //callbacks and the program hangs. Instead, copy the relevant data to the NET byte arrays, 
            //which will be read/polled by a different timer object


            Marshal.Copy(ptr_EventBuffer, NET_EventBuffer, 3, 10);
            NET_EventBuffer[2] = 0x4e;
            BeginInvoke(new EventHandler(delegate { ProcessMessage(NET_EventBuffer); }));

            return true;
        }


        private bool OpenGarmin()
        {

            //Initialise the ANT library and connect to ANT module

            if (ANT_DLL.ANT_Init(0, 115200 - 65536) == false)
            {
                MessageBox.Show("Error initialising ANT module. Ensure the Garmin ANT agent is not running.");
                return false;
            }

            //Set up the callbacks for ANT responses and channel events
            strResponseBuffer = new byte[32];  //needs to be at least 3 bytes
            strChannelEventBuffer = new byte[32]; //needs to be at least 9 bytes

            //Reset wireless transceiver
            ANT_DLL.ANT_ResetSystem();
            Thread.Sleep(50);

            //Pass the callback functions to the ANT_DLL library
            ANT_DLL.ANT_AssignResponseFunction(ptr_ResponseCallback, ptr_ResponseBuffer);
            ANT_DLL.ANT_AssignChannelEventFunction(0, ptr_EventCallback, ptr_EventBuffer);


            //Set network key for Garmin HRM
            //The garmin HRM key is "B9A521FBBD72C345"
            byte[] GarminKey = { 0xb9, 0xa5, 0x21, 0xfb, 0xbd, 0x72, 0xc3, 0x45 };
            ANT_DLL.ANT_SetNetworkKey(0, GarminKey).ToString();
            Thread.Sleep(50);

            //Assign the channel
            //Receive on channel 0, network #0
            ANT_DLL.ANT_AssignChannel(0, 0, 0);
            Thread.Sleep(50);

            //Congifure Channel ID - set up which devices to transmit-receive data from
            //Set to receive from any device it finds
            ushort device_id = ushort.Parse(txtDeviceID.Text);
            
            ANT_DLL.ANT_SetChannelId(0, (ushort)device_id, 0, 0);
            Thread.Sleep(50);

            //Set the receiver search timeout limit
            ANT_DLL.ANT_SetChannelSearchTimeout(0, 0xff);
            Thread.Sleep(50);

            //Set the messaging period (corresponding to the max number of messages per second)
            //Messaging period for Garmin HRM is 0x1f86
            ANT_DLL.ANT_SetChannelPeriod(0, 0x1f86);
            Thread.Sleep(50);

            //Set the radio frequency corresponding to the Garmin watch (frequency 0x39)
            ANT_DLL.ANT_SetChannelRFFreq(0, 0x39);
            Thread.Sleep(50);

            //Open the channel to receive data !
            ANT_DLL.ANT_OpenChannel(0).ToString();

            return true;
        }

        /// <summary>
        /// Close the connection to the Garmin device
        /// </summary>
        private void CloseGarmin()
        {
            ANT_DLL.ANT_CloseChannel(0);
            ANT_DLL.ANT_Close();
        }

        /// <summary>
        /// Open a serial port connection to the SparkFun device, and initialise the ANT receiver
        /// </summary>
        /// <returns></returns>
        private bool OpenSparkfun()
        {
            //Create a new COMport for communications
            if (cbCOMPort.SelectedIndex == -1)
            {
                MessageBox.Show("Please select a COM port");
                return false;
            }
            if (port != null)
            {
                if (port.IsOpen) {
                    port.Close();
                    port.Dispose();
                }
            }
            try
            {
                port = new SerialPort(cbCOMPort.SelectedItem.ToString(), int.Parse(txtBaudRate.Text), Parity.None, 8, StopBits.One);
            }
            catch
            {
                MessageBox.Show("Cannot create communications port.");
                return false;   
            }
            port.DataReceived += new SerialDataReceivedEventHandler(port_DataReceived);
            if (!port.IsOpen)
            {
                try
                {
                    port.Open();
                    terminateSerial = false;
                }
                catch
                {
                    MessageBox.Show("Cannot open communications port.");
                    return false;   
                }
            }


            //Configure device to receive data
            //Write commands to reset the wireless transceiver
            ANT_send(new byte[] { (byte)ANT_msg._0x4A_MESG_SYSTEM_RESET_ID, 0x0 });
            Thread.Sleep(200);
            //Get system capabilities
            ANT_send(new byte[] { (byte)ANT_msg._0x4D_MESG_REQUEST_ID, 0x0, 0x54 });
            Thread.Sleep(50);

            //Mystery request message (see if a suunto device is connected)
            //ANT_send(new byte[] { (byte)ANT_msg._0x4D_MESG_REQUEST_ID, 0x0, 0x3D });
            //Thread.Sleep(50);

            //Set the network key for the Garmin HRM
            //The garmin HRM key is "B9A521FBBD72C345"
            ANT_send(new byte[] { (byte)ANT_msg._0x46_MESG_NETWORK_KEY_ID, 0x00, 0xb9, 0xa5, 0x21, 0xfb, 0xbd, 0x72, 0xc3, 0x45 });
            Thread.Sleep(50);

            //Assign the channel
            //Receive on channel 0, network #1
            ANT_send(new byte[] { (byte)ANT_msg._0x42_MESG_ASSIGN_CHANNEL_ID, 0x0, 0x0, 0x0 });
            Thread.Sleep(50);
            //Congifure Channel ID - set up which devices to transmit-receive data from
            //Set to receive from any device it finds (0) or specific device
            int device_id = int.Parse(txtDeviceID.Text);
            ANT_send(new byte[] { (byte)ANT_msg._0x51_MESG_CHANNEL_ID_ID, 0x0, (byte)(device_id - Math.Floor(device_id / 256.0)*256), (byte)(Math.Floor(device_id / 256.0)), 0x0, 0x0 });
            Thread.Sleep(50);
            //Set the receiver search timeout limit to 10.5 minutes (or infinite in newer devices)
            ANT_send(new byte[] { (byte)ANT_msg._0x44_MESG_CHANNEL_SEARCH_TIMEOUT_ID, 0x00, 0xff });
            Thread.Sleep(50);
            //Set the messaging period (corresponding to the max number of messages per second)
            //Messaging period for Garmin HRM is 0x1f86
            ANT_send(new byte[] { (byte)ANT_msg._0x43_MESG_CHANNEL_MESG_PERIOD_ID, 0x00, 0x86, 0x1f });
            Thread.Sleep(50);
            //Set the radio frequency corresponding to the Garmin watch (frequency 0x39)
            ANT_send(new byte[] { (byte)ANT_msg._0x45_MESG_CHANNEL_RADIO_FREQ_ID, 0x00, 0x39 });
            Thread.Sleep(50);
            //Open the channel to receive data !
            ANT_send(new byte[] { (byte)ANT_msg._0x4B_MESG_OPEN_CHANNEL_ID, 0x00 });
            Thread.Sleep(50);


            return true;
        }

        /// <summary>
        /// Close the serial port connection to the SparkFun device. Set a flag to request device close
        /// instead of calling SerialPort.Close() (as this can cause a thread deadlock and the application to hang)
        /// </summary>
        private void CloseSparkfun() {
            if (port.IsOpen)
            {
                terminateSerial = true;
                //port.Close();
            }
        }


        private void exitToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Application.Exit();
        }

        /// <summary>
        /// Start Manual recording
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnStartRecording_Click(object sender, EventArgs e)
        {
            if (OpenHRM() == true)
            {
                StartRecording();
                btnStopRecording.Enabled = true;
                btnStartRecording.Enabled = false;
                btnStartScript.Enabled = false;
            }
        }

        /// <summary>
        /// Stop Manual recording
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnStopRecording_Click(object sender, EventArgs e)
        {
            StopRecording();
            btnStopRecording.Enabled = false;
            btnStartRecording.Enabled = true;
            btnStartScript.Enabled = true;

            CloseHRM();
        }

        /// <summary>
        /// Start the script recording function
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnStartScript_Click(object sender, EventArgs e)
        {
            StartScript(txtScriptFilename.Text);
        }
        /// <summary>
        /// Show operating system file->open dialog to select script file
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void btnOpenScript_Click(object sender, EventArgs e)
        {
            if (ofdScript.ShowDialog() == DialogResult.OK)
            {
                txtScriptFilename.Text = ofdScript.FileName;
            }
        }

        /// <summary>
        /// Close the form, save settings and release memory
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void frmMain_FormClosing(object sender, FormClosingEventArgs e)
        {
            if (is_recording) StopRecording();
            if (device_open) CloseHRM();

            string settings_path = Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.MyDocuments), "HRV_tracker");
            string filename = Path.Combine(settings_path, "settings.txt");
            StreamWriter sw = new StreamWriter(filename);
            sw.WriteLine(cbDevice.SelectedIndex.ToString());
            sw.WriteLine(txtBaudRate.Text);
            sw.WriteLine(txtDeviceID.Text);
            
            Marshal.FreeHGlobal(ptr_EventBuffer);
            Marshal.FreeHGlobal(ptr_ResponseBuffer);
            
            sw.Close();
        }


        /// <summary>
        /// Query the ANT device for the Channel ID
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void getDeviceIDToolStripMenuItem_Click(object sender, EventArgs e)
        {
            Query_ID();
        }

        /// <summary>
        /// Enable the COM port and baud rate input fields only if the the serial SparkFun device is selected
        /// </summary>
        /// <param name="sender"></param>
        /// <param name="e"></param>
        private void cbDevice_SelectedIndexChanged(object sender, EventArgs e)
        {
            if (cbDevice.SelectedIndex == 0)
            {
                cbCOMPort.Enabled = false;
                txtBaudRate.Enabled = false;
            }
            else
            {
                cbCOMPort.Enabled = true;
                txtBaudRate.Enabled = true;
            }
        }

    }

   
    /// <summary>
    /// Class to wrap routines for accessing the ANT_DLL library
    /// </summary>
    public class ANT_DLL
    {
        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_Init(byte ucUSBDeviceNum, ushort usBaudRate);

        [DllImport("ANT_DLL.dll")]
        public static extern void ANT_Close();

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate bool ANT_ResponseFunctionCallback(byte ucChannel, byte ucResponseMesgID);
        [DllImport("ANT_DLL.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
//        public static extern void ANT_AssignResponseFunction(ANT_ResponseFunctionCallback cb, [MarshalAs(UnmanagedType.LPArray)] byte[] pucResponseBuffer);
        public static extern void ANT_AssignResponseFunction(IntPtr cb, IntPtr pucResponseBuffer);

        [UnmanagedFunctionPointer(CallingConvention.Cdecl)]
        public delegate bool ANT_ChannelEventFunctionCallback(byte ucChannel, byte ucEvent);
        [DllImport("ANT_DLL.dll", CharSet = CharSet.Ansi, CallingConvention = CallingConvention.Cdecl)]
//        public static extern void ANT_AssignChannelEventFunction(byte ucChannel, ANT_ChannelEventFunctionCallback pfChannelEvent, [MarshalAs(UnmanagedType.LPArray)] byte[] pucRxBuffer);
        public static extern void ANT_AssignChannelEventFunction(byte ucChannel, IntPtr pfChannelEvent, IntPtr pucRxBuffer);

        ////////////////////////////////////////////////////////////////////////////////////////
        // Config Messages
        ////////////////////////////////////////////////////////////////////////////////////////
        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_UnAssignChannel(byte ucANTChannel); // Unassign a Channel

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_AssignChannel(byte ucANTChannel, byte ucChanType, byte ucNetNumber);

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_SetChannelId(byte ucANTChannel, ushort usDeviceNumber, byte ucDeviceType, byte ucManufactureID);

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_SetChannelPeriod(byte ucANTChannel, ushort usMesgPeriod);

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_SetChannelSearchTimeout(byte ucANTChannel, byte ucSearchTimeout);   // Sets the search timeout for a give receive channel on the module

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_SetChannelRFFreq(byte ucANTChannel, byte ucRFFreq);

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_SetNetworkKey(byte ucNetNumber, byte[] pucKey);

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_SetTransmitPower(byte ucTransmitPower);

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_AddChannelID(byte ucANTChannel, ushort usDeviceNumber, byte ucDeviceType, byte ucManufactureID, byte ucIndex);

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_ConfigList(byte ucANTChannel, byte ucListSize, byte ucExclude);

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_RxExtMesgsEnable(byte ucEnable);

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_SetLowPriorityChannelSearchTimeout(byte ucANTChannel, byte ucSearchTimeout);

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_SetSerialNumChannelId(byte ucANTChannel, byte ucDeviceType, byte ucManufactureID);

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_EnableLED(byte ucEnable);

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_SetChannelTxPower(byte ucANTChannel, byte ucTransmitPower);

        ////////////////////////////////////////////////////////////////////////////////////////
        // Test Mode
        ////////////////////////////////////////////////////////////////////////////////////////
        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_InitCWTestMode();
        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_SetCWTestMode(byte ucTransmitPower, byte ucRFChannel);

        ////////////////////////////////////////////////////////////////////////////////////////
        // ANT Control messages
        ////////////////////////////////////////////////////////////////////////////////////////
        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_ResetSystem();

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_OpenChannel(byte ucANTChannel); // Opens a Channel

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_CloseChannel(byte ucANTChannel); // Close a channel

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_RequestMessage(byte ucANTChannel, byte ucMessageID);

        [DllImport("ANT_DLL.dll")]
        public static extern bool ANT_OpenRxScanMode(byte ucANTChannel);

        //Wrapper for dealing with _cdecl calls
        public delegate bool ANT_WrapperChannelEventCB(byte ucChannel, byte ucEvent, StringBuilder event_str);
        public delegate bool ANT_WrapperResponseCB(byte ucChannel, byte ucEvent, StringBuilder response_str);

    }

}
