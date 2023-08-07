// Copyright (C) 2022 mocchi
// License: Boost Software License   See LICENSE.txt for the full license.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.Runtime.InteropServices;

namespace PlotWindow_examine
{
	public partial class MainForm : Form
	{
		[DllImport("Module.dll")]
		extern static IntPtr Application_New();
		[DllImport("Module.dll")]
		extern static void Application_Delete(IntPtr app);

		[DllImport("Module.dll", CharSet = CharSet.Ansi)]
		extern static void Application_View_Init(IntPtr app, IntPtr hwnd, string image_filepath);
		[DllImport("Module.dll")]
		extern static void Application_View_Redraw(IntPtr app, IntPtr hwnd, int onpaint);

		// button ...  1:left, 2:right, 4:middle の論理和
		[DllImport("Module.dll")]
		extern static void Application_View_OnMouseDown(IntPtr app, int button, int px, int py, out int invalidate_view);
		[DllImport("Module.dll")]
		extern static void Application_View_OnMouseMove(IntPtr app, int button, int px, int py, out int invalidate_view);
		[DllImport("Module.dll")]
		extern static void Application_View_OnMouseUp(IntPtr app, int button, int px, int py, out int invalidate_view);
		[DllImport("Module.dll")]
		extern static void Application_View_OnMouseWheel(IntPtr app, double delta, int px, int py, out int invalidate_view);

		IntPtr _app;
		public MainForm(string image_filepath) {
			_app = Application_New();
			InitializeComponent();
			Application_View_Init(_app, panelView.Handle, image_filepath);
			this.panelView.MouseWheel += new System.Windows.Forms.MouseEventHandler(this.PanelView_MouseWheel);
		}

		private void MainForm_FormClosed(object sender, FormClosedEventArgs e) {
			Application_Delete(_app);
		}

		private void PanelView_Paint(object sender, PaintEventArgs e) {
			Application_View_Redraw(_app, panelView.Handle, 0);
		}

		private void PanelView_Resize(object sender, EventArgs e) {
			Application_View_Redraw(_app, panelView.Handle, 0);
		}

		private void PanelView_MouseEnter(object sender, EventArgs e) {
			Focus();
		}

		private void PanelView_MouseLeave(object sender, EventArgs e) {
			ActiveControl = null;
		}

		int ConvertMouseButton(MouseButtons mb) {
			return
				((mb & MouseButtons.Left) == MouseButtons.Left ? 1 : 0) |
				((mb & MouseButtons.Right) == MouseButtons.Right ? 2 : 0) |
				((mb & MouseButtons.Middle) == MouseButtons.Middle ? 4 : 0);
		}
		private void PanelView_MouseDown(object sender, MouseEventArgs e) {
			int invalidate_view;
			Application_View_OnMouseDown(_app, ConvertMouseButton(e.Button), e.X, e.Y, out invalidate_view);
			if (invalidate_view != 0) Application_View_Redraw(_app, panelView.Handle, 0);
		}

		private void PanelView_MouseMove(object sender, MouseEventArgs e) {
			int invalidate_view;
			Application_View_OnMouseMove(_app, ConvertMouseButton(e.Button), e.X, e.Y, out invalidate_view);
			if (invalidate_view != 0) Application_View_Redraw(_app, panelView.Handle, 0);
		}

		private void PanelView_MouseUp(object sender, MouseEventArgs e) {
			int invalidate_view;
			Application_View_OnMouseUp(_app, ConvertMouseButton(e.Button), e.X, e.Y, out invalidate_view);
			if (invalidate_view != 0) Application_View_Redraw(_app, panelView.Handle, 0);
		}

		private void PanelView_MouseWheel(object sender, MouseEventArgs e) {
			int invalidate_view;
			Application_View_OnMouseWheel(_app, (double)e.Delta / 120.0, e.X, e.Y, out invalidate_view);
			if (invalidate_view != 0) Application_View_Redraw(_app, panelView.Handle, 0);
		}
	}
}
