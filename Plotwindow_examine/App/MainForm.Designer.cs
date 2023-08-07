namespace PlotWindow_examine
{
	partial class MainForm
	{
		/// <summary>
		/// 必要なデザイナー変数です。
		/// </summary>
		private System.ComponentModel.IContainer components = null;

		/// <summary>
		/// 使用中のリソースをすべてクリーンアップします。
		/// </summary>
		/// <param name="disposing">マネージド リソースを破棄する場合は true を指定し、その他の場合は false を指定します。</param>
		protected override void Dispose(bool disposing)
		{
			if (disposing && (components != null))
			{
				components.Dispose();
			}
			base.Dispose(disposing);
		}

		#region Windows フォーム デザイナーで生成されたコード

		/// <summary>
		/// デザイナー サポートに必要なメソッドです。このメソッドの内容を
		/// コード エディターで変更しないでください。
		/// </summary>
		private void InitializeComponent()
		{
			this.panelView = new System.Windows.Forms.Panel();
			this.SuspendLayout();
			// 
			// panelView
			// 
			this.panelView.Anchor = ((System.Windows.Forms.AnchorStyles)((((System.Windows.Forms.AnchorStyles.Top | System.Windows.Forms.AnchorStyles.Bottom) 
            | System.Windows.Forms.AnchorStyles.Left) 
            | System.Windows.Forms.AnchorStyles.Right)));
			this.panelView.Location = new System.Drawing.Point(54, 41);
			this.panelView.Margin = new System.Windows.Forms.Padding(0);
			this.panelView.Name = "panelView";
			this.panelView.Size = new System.Drawing.Size(737, 383);
			this.panelView.TabIndex = 0;
			this.panelView.Paint += new System.Windows.Forms.PaintEventHandler(this.PanelView_Paint);
			this.panelView.MouseDown += new System.Windows.Forms.MouseEventHandler(this.PanelView_MouseDown);
			this.panelView.MouseEnter += new System.EventHandler(this.PanelView_MouseEnter);
			this.panelView.MouseLeave += new System.EventHandler(this.PanelView_MouseLeave);
			this.panelView.MouseMove += new System.Windows.Forms.MouseEventHandler(this.PanelView_MouseMove);
			this.panelView.MouseUp += new System.Windows.Forms.MouseEventHandler(this.PanelView_MouseUp);
			this.panelView.Resize += new System.EventHandler(this.PanelView_Resize);
			// 
			// MainForm
			// 
			this.AutoScaleDimensions = new System.Drawing.SizeF(6F, 12F);
			this.AutoScaleMode = System.Windows.Forms.AutoScaleMode.Font;
			this.ClientSize = new System.Drawing.Size(800, 450);
			this.Controls.Add(this.panelView);
			this.Name = "MainForm";
			this.Text = "PlotWindow_examine";
			this.FormClosed += new System.Windows.Forms.FormClosedEventHandler(this.MainForm_FormClosed);
			this.ResumeLayout(false);

		}

		#endregion

		private System.Windows.Forms.Panel panelView;
	}
}

