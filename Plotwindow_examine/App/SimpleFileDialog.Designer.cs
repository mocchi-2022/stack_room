namespace PlotWindow_examine {
	partial class SimpleFileDialog {
		/// <summary>
		/// 必要なデザイナー変数です。
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// 使用中のリソースをすべてクリーンアップします。
		/// </summary>
		/// <param name="disposing">マネージ リソースが破棄される場合 true、破棄されない場合は false です。</param>
		protected override void Dispose(bool disposing) {
			if (disposing && (components != null)) {
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows フォーム デザイナーで生成されたコード

		/// <summary>
		/// デザイナー サポートに必要なメソッドです。このメソッドの内容を
		/// コード エディターで変更しないでください。
		/// </summary>
		private void InitializeComponent() {
			this.textBoxTargetFolder = new System.Windows.Forms.TextBox();
			this.buttonUndo = new System.Windows.Forms.Button();
			this.buttonRedo = new System.Windows.Forms.Button();
			this.buttonUp = new System.Windows.Forms.Button();
			this.listViewFileList = new System.Windows.Forms.ListView();
			this.label1 = new System.Windows.Forms.Label();
			this.textBoxFileName = new System.Windows.Forms.TextBox();
			this.comboBoxFilter = new System.Windows.Forms.ComboBox();
			this.buttonOK = new System.Windows.Forms.Button();
			this.buttonCancel = new System.Windows.Forms.Button();
			this.radioButtonList = new System.Windows.Forms.RadioButton();
			this.radioButtonDetail = new System.Windows.Forms.RadioButton();
			this.panel1 = new System.Windows.Forms.Panel();
			this.buttonRedraw = new System.Windows.Forms.Button();
			this.panel1.SuspendLayout();
			this.SuspendLayout();
			// 
			// textBoxTargetFolder
			// 
			this.textBoxTargetFolder.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.textBoxTargetFolder.Location = new System.Drawing.Point(87, 11);
			this.textBoxTargetFolder.Name = "textBoxTargetFolder";
			this.textBoxTargetFolder.Size = new System.Drawing.Size(461, 19);
			this.textBoxTargetFolder.TabIndex = 0;
			this.textBoxTargetFolder.Enter += new System.EventHandler(this.textBoxTargetFolder_Enter);
			this.textBoxTargetFolder.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.textBoxTargetFolder_KeyPress);
			// 
			// buttonUndo
			// 
			this.buttonUndo.Enabled = false;
			this.buttonUndo.Location = new System.Drawing.Point(5, 8);
			this.buttonUndo.Name = "buttonUndo";
			this.buttonUndo.Size = new System.Drawing.Size(25, 25);
			this.buttonUndo.TabIndex = 1;
			this.buttonUndo.UseVisualStyleBackColor = true;
			this.buttonUndo.Click += new System.EventHandler(this.buttonUndo_Click);
			// 
			// buttonRedo
			// 
			this.buttonRedo.Enabled = false;
			this.buttonRedo.Location = new System.Drawing.Point(31, 8);
			this.buttonRedo.Name = "buttonRedo";
			this.buttonRedo.Size = new System.Drawing.Size(25, 25);
			this.buttonRedo.TabIndex = 1;
			this.buttonRedo.UseVisualStyleBackColor = true;
			this.buttonRedo.Click += new System.EventHandler(this.buttonRedo_Click);
			// 
			// buttonUp
			// 
			this.buttonUp.Location = new System.Drawing.Point(58, 8);
			this.buttonUp.Name = "buttonUp";
			this.buttonUp.Size = new System.Drawing.Size(25, 25);
			this.buttonUp.TabIndex = 1;
			this.buttonUp.UseVisualStyleBackColor = true;
			this.buttonUp.Click += new System.EventHandler(this.buttonUp_Click);
			// 
			// listViewFileList
			// 
			this.listViewFileList.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.listViewFileList.FullRowSelect = true;
			this.listViewFileList.Location = new System.Drawing.Point(5, 38);
			this.listViewFileList.MultiSelect = false;
			this.listViewFileList.Name = "listViewFileList";
			this.listViewFileList.Size = new System.Drawing.Size(659, 240);
			this.listViewFileList.TabIndex = 2;
			this.listViewFileList.UseCompatibleStateImageBehavior = false;
			this.listViewFileList.VirtualMode = true;
			this.listViewFileList.RetrieveVirtualItem += new System.Windows.Forms.RetrieveVirtualItemEventHandler(this.listViewFileList_RetrieveVirtualItem);
			this.listViewFileList.Click += new System.EventHandler(this.listViewFileList_Click);
			this.listViewFileList.DoubleClick += new System.EventHandler(this.listViewFileList_DoubleClick);
			// 
			// label1
			// 
			this.label1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left)));
			this.label1.AutoSize = true;
			this.label1.Location = new System.Drawing.Point(15, 286);
			this.label1.Name = "label1";
			this.label1.Size = new System.Drawing.Size(48, 12);
			this.label1.TabIndex = 4;
			this.label1.Text = "filename";
			// 
			// textBoxFileName
			// 
			this.textBoxFileName.Anchor = ((System.Windows.Forms.AnchorStyles)(((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.textBoxFileName.Location = new System.Drawing.Point(69, 282);
			this.textBoxFileName.Name = "textBoxFileName";
			this.textBoxFileName.Size = new System.Drawing.Size(446, 19);
			this.textBoxFileName.TabIndex = 0;
			this.textBoxFileName.KeyPress += new System.Windows.Forms.KeyPressEventHandler(this.textBoxFileName_KeyPress);
			// 
			// comboBoxFilter
			// 
			this.comboBoxFilter.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.comboBoxFilter.DropDownStyle = System.Windows.Forms.ComboBoxStyle.DropDownList;
			this.comboBoxFilter.FormattingEnabled = true;
			this.comboBoxFilter.Location = new System.Drawing.Point(521, 282);
			this.comboBoxFilter.Name = "comboBoxFilter";
			this.comboBoxFilter.Size = new System.Drawing.Size(143, 20);
			this.comboBoxFilter.TabIndex = 5;
			this.comboBoxFilter.SelectedValueChanged += new System.EventHandler(this.comboBoxFilter_SelectedValueChanged);
			// 
			// buttonOK
			// 
			this.buttonOK.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.buttonOK.Location = new System.Drawing.Point(509, 306);
			this.buttonOK.Name = "buttonOK";
			this.buttonOK.Size = new System.Drawing.Size(75, 23);
			this.buttonOK.TabIndex = 6;
			this.buttonOK.Text = "Save";
			this.buttonOK.UseVisualStyleBackColor = true;
			this.buttonOK.Click += new System.EventHandler(this.buttonOK_Click);
			// 
			// buttonCancel
			// 
			this.buttonCancel.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Bottom | System.Windows.Forms.AnchorStyles.Right)));
			this.buttonCancel.DialogResult = System.Windows.Forms.DialogResult.Cancel;
			this.buttonCancel.Location = new System.Drawing.Point(590, 306);
			this.buttonCancel.Name = "buttonCancel";
			this.buttonCancel.Size = new System.Drawing.Size(75, 23);
			this.buttonCancel.TabIndex = 6;
			this.buttonCancel.Text = "Cancel";
			this.buttonCancel.UseVisualStyleBackColor = true;
			this.buttonCancel.Click += new System.EventHandler(this.buttonCancel_Click);
			// 
			// radioButtonList
			// 
			this.radioButtonList.Appearance = System.Windows.Forms.Appearance.Button;
			this.radioButtonList.Location = new System.Drawing.Point(3, 2);
			this.radioButtonList.Margin = new System.Windows.Forms.Padding(0);
			this.radioButtonList.Name = "radioButtonList";
			this.radioButtonList.Size = new System.Drawing.Size(25, 25);
			this.radioButtonList.TabIndex = 7;
			this.radioButtonList.UseVisualStyleBackColor = true;
			this.radioButtonList.CheckedChanged += new System.EventHandler(this.radioButtonList_CheckedChanged);
			// 
			// radioButtonDetail
			// 
			this.radioButtonDetail.Appearance = System.Windows.Forms.Appearance.Button;
			this.radioButtonDetail.Checked = true;
			this.radioButtonDetail.Location = new System.Drawing.Point(30, 2);
			this.radioButtonDetail.Margin = new System.Windows.Forms.Padding(0);
			this.radioButtonDetail.Name = "radioButtonDetail";
			this.radioButtonDetail.Size = new System.Drawing.Size(25, 25);
			this.radioButtonDetail.TabIndex = 7;
			this.radioButtonDetail.TabStop = true;
			this.radioButtonDetail.UseVisualStyleBackColor = true;
			this.radioButtonDetail.CheckedChanged += new System.EventHandler(this.radioButtonDetails_CheckedChanged);
			// 
			// panel1
			// 
			this.panel1.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.panel1.Controls.Add(this.radioButtonList);
			this.panel1.Controls.Add(this.radioButtonDetail);
			this.panel1.Location = new System.Drawing.Point(604, 11);
			this.panel1.Name = "panel1";
			this.panel1.Size = new System.Drawing.Size(60, 27);
			this.panel1.TabIndex = 8;
			// 
			// buttonRedraw
			// 
			this.buttonRedraw.Anchor = ((System.Windows.Forms.AnchorStyles)((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Right)));
			this.buttonRedraw.Location = new System.Drawing.Point(551, 8);
			this.buttonRedraw.Name = "buttonRedraw";
			this.buttonRedraw.Size = new System.Drawing.Size(25, 25);
			this.buttonRedraw.TabIndex = 9;
			this.buttonRedraw.UseVisualStyleBackColor = true;
			this.buttonRedraw.Click += new System.EventHandler(this.buttonRedraw_Click);
			// 
			// SimpleFileDialog
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 12F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.CancelButton = this.buttonCancel;
			this.ClientSize = new System.Drawing.Size(674, 340);
			this.Controls.Add(this.buttonRedraw);
			this.Controls.Add(this.panel1);
			this.Controls.Add(this.buttonCancel);
			this.Controls.Add(this.buttonOK);
			this.Controls.Add(this.comboBoxFilter);
			this.Controls.Add(this.label1);
			this.Controls.Add(this.listViewFileList);
			this.Controls.Add(this.buttonUp);
			this.Controls.Add(this.buttonRedo);
			this.Controls.Add(this.buttonUndo);
			this.Controls.Add(this.textBoxFileName);
			this.Controls.Add(this.textBoxTargetFolder);
			this.KeyPreview = true;
			this.MaximizeBox = false;
			this.MinimizeBox = false;
			this.Name = "SimpleFileDialog";
			this.ShowInTaskbar = false;
			this.Text = "SimpeFileDialog";
			this.FormClosing += new System.Windows.Forms.FormClosingEventHandler(this.SimpleFileDialog_FormClosing);
			this.Load += new System.EventHandler(this.SimpleFileDialog_Load);
			this.panel1.ResumeLayout(false);
			this.ResumeLayout(false);
			this.PerformLayout();

		}

		#endregion

		private System.Windows.Forms.TextBox textBoxTargetFolder;
		private System.Windows.Forms.Button buttonUndo;
		private System.Windows.Forms.Button buttonRedo;
		private System.Windows.Forms.Button buttonUp;
		private System.Windows.Forms.ListView listViewFileList;
		private System.Windows.Forms.Label label1;
		private System.Windows.Forms.TextBox textBoxFileName;
		private System.Windows.Forms.ComboBox comboBoxFilter;
		private System.Windows.Forms.Button buttonOK;
		private System.Windows.Forms.Button buttonCancel;
		private System.Windows.Forms.RadioButton radioButtonList;
		private System.Windows.Forms.RadioButton radioButtonDetail;
		private System.Windows.Forms.Panel panel1;
		private System.Windows.Forms.Button buttonRedraw;
	}
}

