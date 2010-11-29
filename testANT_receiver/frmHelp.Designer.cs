namespace HRV_tracker
{
    partial class frmHelp
    {
        /// <summary>
        /// Required designer variable.
        /// </summary>
        private System.ComponentModel.IContainer components = null;

        /// <summary>
        /// Clean up any resources being used.
        /// </summary>
        /// <param name="disposing">true if managed resources should be disposed; otherwise, false.</param>
        protected override void Dispose(bool disposing)
        {
            if (disposing && (components != null))
            {
                components.Dispose();
            }
            base.Dispose(disposing);
        }

        #region Windows Form Designer generated code

        /// <summary>
        /// Required method for Designer support - do not modify
        /// the contents of this method with the code editor.
        /// </summary>
        private void InitializeComponent()
        {
            this.wbHelp = new System.Windows.Forms.WebBrowser();
            this.SuspendLayout();
            // 
            // wbHelp
            // 
            this.wbHelp.Dock = System.Windows.Forms.DockStyle.Fill;
            this.wbHelp.Location = new System.Drawing.Point(0, 0);
            this.wbHelp.MinimumSize = new System.Drawing.Size(20, 20);
            this.wbHelp.Name = "wbHelp";
            this.wbHelp.Size = new System.Drawing.Size(996, 498);
            this.wbHelp.TabIndex = 0;
            // 
            // frmHelp
            // 
            this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 13F);
            this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
            this.ClientSize = new System.Drawing.Size(996, 498);
            this.Controls.Add(this.wbHelp);
            this.Name = "frmHelp";
            this.Text = "HRV_tracker help";
            this.ResumeLayout(false);

        }

        #endregion

        public System.Windows.Forms.WebBrowser wbHelp;

    }
}