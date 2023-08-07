using System;
using System.Collections.Generic;
using System.Linq;
using System.Threading.Tasks;
using System.Windows.Forms;

namespace PlotWindow_examine
{
	static class Program
	{
		/// <summary>
		/// アプリケーションのメイン エントリ ポイントです。
		/// </summary>
		[STAThread]
		static void Main()
		{
			Application.EnableVisualStyles();
			Application.SetCompatibleTextRenderingDefault(false);

			var ofn = new SimpleFileDialog(SimpleFileDialog.FileDialogMode.OpenFile);
			ofn.CheckFileExists = true;
			ofn.Filter = "All images(*.png,*.hdr,*.jpeg)|*.png;*.hdr;*.jpeg";
			ofn.FilterIndex = 0;
			if (ofn.ShowDialog() != DialogResult.OK) return;

			Application.Run(new MainForm(ofn.FileName));
		}
	}
}
