using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Windows.Forms;
using System.IO;

namespace PExplorer
{
    public partial class frmMain : Form
    {
        Byte[] image;
        int e_line;
        int imgsize;

        const int TAPE_START = 0x4009;

        String[] token = 
        {
	        " ", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "\"", "`", "$", ":",
	        "?", "(", ")", ">", "<", "=", "+", "-", "*", "/", ";", ",",  ".", "0", "1",
	        "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D",  "E", "F", "G",
	        "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S",  "T", "U", "V",
	        "W", "X", "Y", "Z", "RND", "INKEY$", "PI",
	        "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", 
	        "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", 
	        "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", 
	        "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", 
	        "?",
	        " ", "?", "?", "?", "?", "?", "?", "?", "?", "?", "?", "\"", "`", "$", ":",
	        "?", "(", ")", ">", "<", "=", "+", "-", "*", "/", ";", ",",  ".", "0", "1",
	        "2", "3", "4", "5", "6", "7", "8", "9", "A", "B", "C", "D",  "E", "F", "G",
	        "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S",  "T", "U", "V",
	        "W", "X", "Y", "Z",
	        "\"\"", "AT ", "TAB ", "?", "CODE ", "VAL ", "LEN ", "SIN ", "COS ", "TAN ",
	        "ASN ", "ACS ", "ATN ", "LN ", "EXP ", "INT ", "SQR ", "SGN ", "ABS ",
	        "PEEK ", "USR ", "STR$ ", "CHR$ ", "NOT ", "**", " OR ", " AND ", "<=", ">=",
	        "<>", " THEN", " TO ", " STEP ", " LPRINT ", " LLIST ", " STOP ", " SLOW ", 
	        " FAST ", " NEW ", " SCROLL ", " CONT ", " DIM ", " REM ", " FOR ", " GOTO ",
	        " GOSUB ", " INPUT ", " LOAD ", " LIST ", " LET ", " PAUSE ", " NEXT ",
	        " POKE ", " PRINT ", " PLOT ", " RUN ", " SAVE ", " RAND ", " IF ", " CLS ",
	        " UNPLOT ", " CLEAR ", " RETURN ", " COPY "
        };

        public frmMain()
        {
            InitializeComponent();
            clear();
        }

        private void clear()
        {
            lblSize.Text = "";
            lbBasic.Items.Clear();
            imgsize = 0;
            btnExport.Enabled = false;
        }

        private void btnBrowse_Click(object sender, EventArgs e)
        {
            FileDialog fd = new OpenFileDialog();
            fd.AddExtension = true;
            fd.DefaultExt = "p";
            fd.Filter = "ZX81 file (*.p)|*.p";
            if (fd.ShowDialog() == DialogResult.OK)
            {
                txtFilename.Text = fd.FileName;
            }
        }

        private int getIndex(string hexaddr)
        {
            return int.Parse(hexaddr, System.Globalization.NumberStyles.HexNumber) - TAPE_START;
        }

        private int getWord(string hexaddr)
        {
            int pos = getIndex(hexaddr);
            return image[pos] + 256 * image[pos + 1];
        }

        private void btnOpen_Click(object sender, EventArgs e)
        {
            if (File.Exists(txtFilename.Text)) {
                clear();
                image = File.ReadAllBytes(txtFilename.Text);
                bool valid = (image.Length > 120) && (image[0] == 0);
                if (valid) {
                    e_line = getWord("4014");
                    imgsize = e_line - TAPE_START;
                    if (imgsize <= image.Length)
                    {
                        valid = image[imgsize - 1] == 0x80;
                    }
                    else
                    {
                        valid = false;
                    }
                }
                if (valid) {
                    lblSize.Text = "Size: " + imgsize.ToString() + " bytes";
                    decodBasic();
                    btnExport.Enabled = true;
                } else {
                    MessageBox.Show("Invalid file!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
                }
            } else {
                MessageBox.Show("File not found!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
            }
        }

        private void decodBasic()
        {
            int start = getIndex("407D");
            int end = getWord("400C") - TAPE_START;
            for (int pos = start; pos < end; )
            {
                int lineno = (image[pos] * 256) + image[pos + 1];
                int linesz = (image[pos+3] * 256) + image[pos + 2];
                pos += 4;
                StringBuilder sbLine = new StringBuilder();
                sbLine.Append(lineno.ToString()+" ");
                for (int i = 0; i < linesz-1;)
                {
                    //sbLine.Append(String.Format(" {0:X2}", image[pos]));
                    if (image[pos] == 0xEA) // REM
                    {
                        while (i < linesz - 1)
                        {
                            sbLine.Append(token[image[pos++]]);
                            i++;
                        }
                    } else if (image[pos] == 0x7E) // ignore binary float value
                    {
                        pos += 6;
                        i += 6;
                    }
                    else
                    {
                        sbLine.Append(token[image[pos]]);
                        pos++;
                        i++;
                    }
                }
                if (image[pos] != 0x76)
                {
                    MessageBox.Show("Bad Code!", "Error", MessageBoxButtons.OK, MessageBoxIcon.Exclamation);
                }
                pos++;
                lbBasic.Items.Add(sbLine.ToString());
            }
        }

        private void btnExport_Click(object sender, EventArgs e)
        {
            String expfile = Path.ChangeExtension(txtFilename.Text, "h");
            if (File.Exists(expfile))
            {
                File.Delete(expfile);
            }
            using (StreamWriter sw = new StreamWriter(expfile))
            {
                sw.WriteLine("const uint8_t code[] =  {");
                StringBuilder sb = new StringBuilder();
                sb.Append("  ");
                int i;
                for (i = 0; i < imgsize; i++)
                {
                    sb.Append(String.Format(" 0x{0:X2},", image[i]));
                    if ((i % 16) == 15) {
                        if (i == (imgsize - 1))
                        {
                            sb.Remove(sb.Length-1, 1);
                        }
                        sw.WriteLine(sb.ToString());
                        sb = new StringBuilder();
                        sb.Append("  ");
                    }
                }
                if (sb.Length > 2)
                {
                    sb.Remove(sb.Length-1, 1);
                    sw.WriteLine(sb.ToString());
                }
                sw.WriteLine("};");
            }
            MessageBox.Show("Exported to " + expfile, "Info", MessageBoxButtons.OK, MessageBoxIcon.Information);
        }
    }
}
