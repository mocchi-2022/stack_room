/// Simple File Dialog version 0.4
/// Copylight mocchi 2021
/// mocchi_2003@yahoo.co.jp
/// Distributed under the Boost Software License, Version 1.0.

using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.Linq;
using System.Text;
using System.Text.RegularExpressions;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO;

// references
// https://www.codeproject.com/Articles/13097/An-quot-Explorer-Style-quot-TreeView-Control
// https://www.ipentec.com/document/csharp-shell-namespace-create-explorer-tree-view-control-and-linked-list-view
// http://acha-ya.cocolog-nifty.com/blog/2010/09/post-241a.html
// https://nasu38yen.wordpress.com/2010/05/28/%e6%8b%a1%e5%bc%b5%e5%ad%90%e3%81%8b%e3%82%89%e5%b0%8f%e3%81%95%e3%81%aa%e3%82%a2%e3%82%a4%e3%82%b3%e3%83%b3%e3%82%92get%e3%81%99%e3%82%8b%e3%81%ab%e3%81%af%e3%80%81shgetfileinfo%e3%82%92usefileattrib/
// https://dobon.net/vb/bbs/log3-51/30394.html
// https://www.curict.com/item/0a/0a33f42.html
// https://code-examples.net/ja/q/137a22b

// Todo: select folder
namespace PlotWindow_examine {
	public partial class SimpleFileDialog : Form {

		// properties
		public string InitialDirectory {
			get;
			set;
		}
		public string FileName {
			get;
			set;
		}
		public string[] FileNames {
			get;
			private set;
		}
		public string Filter {
			get;
			set;
		}
		public string Title {
			get;
			set;
		}
		/// <summary>
		/// Valid only "OpenFile" Mode
		/// </summary>
		public bool Multiselect {
			get;
			set;
		}
		/// <summary>
		/// Valid only "OpenFile" Mode
		/// </summary>
		public bool CheckFileExists {
			get;
			set;
		}
		/// <summary>
		/// Valid only "SaveFile" Mode
		/// </summary>
		public bool OverwritePrompt {
			get;
			set;
		}
		private string _defaultExt = "";
		public string DefaultExt {
			get { return _defaultExt; }
			set {
				if (value.Length > 0 && value[0] == '.') {
					_defaultExt = value.Substring(1);
				} else {
					_defaultExt = value;
				}
			}
		}
		public int FilterIndex {
			get;
			set;
		}

		private FileDialogMode _fileDialogMode;
		public enum FileDialogMode {
			OpenFile, SaveFile, SelectFolder
		}

		private string _currentDirectory = "";
		public string CurrentDirectory {
			get {
				return _currentDirectory;
			}
			set {
				if (!Directory.Exists(value)) {
					throw new DirectoryNotFoundException();
				}
				if (_currentDirectory == value) return;

				try {
					var dirs = Directory.EnumerateDirectories(value);
				} catch {
					MessageBox.Show("access denied", "error", MessageBoxButtons.OK, MessageBoxIcon.Error);
					return;
				}
				_currentDirectory = value;

				// update undo&redo buffer
				if (_undoRedoBuffer != null) {
					if (_undoIndex < _undoRedoBuffer.Count - 1) _undoRedoBuffer.RemoveRange(_undoIndex + 1, _undoRedoBuffer.Count - _undoIndex - 1);
					_undoRedoBuffer.Add(_currentDirectory);
					++_undoIndex;
					if (_undoIndex > 0) buttonUndo.Enabled = true;
					buttonRedo.Enabled = false;
				}

				RedrawListView();
			}
		}

		private string _customFilterString;
		private int _undoIndex;
		private List<string> _undoRedoBuffer;

		public SimpleFileDialog(FileDialogMode fileDialogMode = FileDialogMode.OpenFile) {
			Filter = "";
			_fileDialogMode = fileDialogMode;
			_undoRedoBuffer = null;
			InitializeComponent();

			switch (fileDialogMode) {
				case FileDialogMode.OpenFile:
					buttonOK.Text = "Open";
					CheckFileExists = true;
					break;
				case FileDialogMode.SaveFile:
					buttonOK.Text = "Save";
					OverwritePrompt = true;
					break;
				case FileDialogMode.SelectFolder:
					buttonOK.Text = "Select";
					textBoxFileName.ReadOnly = true;
					textBoxFileName.Text = "Select Folder.";
					break;
			}

			// dialog icons
			var leftArrowBmp = CreateBitmapFromBase64(@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAABnRSTlMAwwDDAMNRly6JAAAAbUlEQVR42mM8fPgwAymAkVIN/TP4gOTh/Q3rVhYR1kBQNYoGYlQjNBCpGqoBopogKMz4BNIgKhVo69hAjAaI/aRrADopKLwPogcohF8PVAOQBdcDcSjhUCJeD0rEEaMHPWkA9cCdS5QGgoBkDQBaUFah4+0qJgAAAABJRU5ErkJggg==");
			buttonUndo.Image = leftArrowBmp;

			var rightArrowBmp = CreateBitmapFromBase64(@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAABnRSTlMAwwDDAMNRly6JAAAAaklEQVR42mM8fPgwAymAkWoa+mfwAcl1S/XRFOCzAaseAk7C1EPYD2h6QBogQgQBRA8JGg7vb1i3sogsDUBOUHgfLnW2jg1w1cSGElw1UfGArJqBYEyjqcanwdbWVlQqEE01YT9gApI1AAASUl+h+XaRoQAAAABJRU5ErkJggg==");
			buttonRedo.Image = rightArrowBmp;

			var upArrowBmp = CreateBitmapFromBase64(@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAABnRSTlMAwwDDAMNRly6JAAAAbklEQVR42mM8fPgwAymAEVND/ww+COPw/oZ1K4sIaICrxqUHRQOaaqx6EBogqoHSto4NcKUQNrIeqAa4aqAEsh+AJJoehAa4EJqng8L7gHrWLdWHqIRqAIrCLcUMJWRZioN1pGqAhCOEQawGPAAA17yHoW6n2a8AAAAASUVORK5CYII=");
			buttonUp.Image = upArrowBmp;

			var reloadBmp = CreateBitmapFromBase64(@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAABnRSTlMAwwDDAMNRly6JAAAAlklEQVR42mM8fPgwAzYQ/64TSC4UKkcTZ8SjYbLpxdzT+mh6UDRATIUDoAYgiaYHoQFiJFbbvKMeH24+jKIBj2rCGoDSEMbWZbJoqqEa0FRDpG1rbYEa0FSja0CWBmoAkmiq8WnAGnTA4CJKA7IsZRowowkzPKDBCgkTuB60yEa2HBHTyHoIRxwePVgiDlkaEvzIAC0YAOE3raHJ0QpxAAAAAElFTkSuQmCC");
			buttonRedraw.Image = reloadBmp;

			var listBmp = CreateBitmapFromBase64(@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAPElEQVR42mOsr69nIAUwAjU0NDQQqRqoEqrBwfMskH9guzHJGhgZGbEq/f//P4qGUSeNFCeRpoFI1RAAAKkAZeGr5RELAAAAAElFTkSuQmCC");
			radioButtonList.Image = listBmp;

			var detailBmp = CreateBitmapFromBase64(@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAUUlEQVR42mOsr69nIAUwAjW4uLhgSuzZswcuDmcDGWRpOHDSF8g5sN2YoHsaGhpQNDAyMsLl/v//D+fC2UDFIA1AfUT6GN2GUScNLScRqRoCAK43deFZIhv/AAAAAElFTkSuQmCC");
			radioButtonDetail.Image = detailBmp;

		}

		private Dictionary<string, int> _extensionToImageListIndex;

		private class ChildItem {
			public bool isDir;
			public string text, dateModified, fileSize;
			public int imageIndex;
			public ChildItem(bool isDir, string text, string dateModified, string fileSize, int imageIndex) {
				this.isDir = isDir;
				this.text = text;
				this.dateModified = dateModified;
				this.fileSize = fileSize;
				this.imageIndex = imageIndex;
			}
		}
		private ChildItem[] _children;

		private void SimpleFileDialog_Load(object sender, EventArgs e) {

			if (!string.IsNullOrEmpty(Title)) {
				Text = Title;
			} else if (_fileDialogMode == FileDialogMode.OpenFile){
				Text = "Open";
				OverwritePrompt = false;
			} else if (_fileDialogMode == FileDialogMode.SaveFile) {
				Text = "Save as";
				Multiselect = false;
				CheckFileExists = false;
			} else if (_fileDialogMode == FileDialogMode.SelectFolder) {
				Text = "Select folder";
				Multiselect = false;
				CheckFileExists = false;
				OverwritePrompt = false;
			}

			if (string.IsNullOrEmpty(FileName)){
				FileName = "";
			}
			FileNames = new string[] { FileName };


			// folder / file icon
			var imageList = new ImageList();
			var iconsInfo = new[]{
				// === special icons ===
				// folder
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAABnRSTlMAAAAAAABupgeRAAAARklEQVR42mP8//8/AymAkRwNl1cxo4nqhv0lTYO2WRqmUiaF6VAN/x5kEuOYPbPfu7WuGNUweDQAObuqI4jRg9BAPCBZAwB33G7hdlLspwAAAABJRU5ErkJggg==",
				},
				// any file
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAATklEQVR42mP8//8/AymAEaihoaEBvyJkBVANuPQAxV1cXPbs2QNXQJQGIMPW1hbieMIaIIzGxkaiNCCczsg4qmGwaQByGIgACA3EqIYDANMbhuGW3OtEAAAAAElFTkSuQmCC",
				},

				// === icons for each extensions from here ===
				// 3d shape
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAs0lEQVR42mP8//8/AymAEaihoaEBvyJkBVANuPQAxV1cXPbs2QNXgKEhuwlFg+g/oAYgw9bWFuJ4VA3ZTZ+qYpE19M1eCGE0NjZiaMBQDQHTZczL/79iZGTEogHIR9PD17a4c9oUnBqAqoEqkDXsErh2vm0fPg0QdS8uHgGSD45uVbD2Xu9dTEADULWEvg1cz0DagBQPaKpB6rBrYGDoZBRDDiWIanQNQA4x6RShgdiUDQYA2nzn4SxnIr4AAAAASUVORK5CYII=",
					".stl", ".ply", ".wrl", ".fbx", ".3mf", ".glb", ".3dm", ".stp", ".step", ".igs", ".iges", ".3ds", ".blend"
				},
				// archive
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAcklEQVR42mP8//8/AymAkRwNl1cxo4nqhv0lTYO2WRqmUiaF6VAN/x5kEuOYPbPfu7WuIEtDQ0ODi4vLnj178JC2trY7q8KhGo4cOUJQNcQSoGKinMSsOAOklJGRlhpIcxLFwQrk7KqOIEYPQgPxgGQNABK3kuF10wpFAAAAAElFTkSuQmCC",
					".zip", ".tar", ".gz", ".tgz", ".bz2", ".tbz", ".lzh", ".7z", ".rar", ".msi"
				},
				// data
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAY0lEQVR42mP8//8/AymAEaihoaEBvyJkBVANuPQAxV1cXPbs2QNXQJQGIMPW1hbieIQGRkZGTA319fUQRmNjI1EaIACoBiiLRQOKBCoDu4aRZwOBJESRBjyOQXMYVAMxquEAAAQVtuHe59v8AAAAAElFTkSuQmCC",
					".csv", ".xml", ".json", ".db", ".db3", ".sqlite", ".sqlite3", ".sqlitedb", ".mdb", ".dat"
				},
				// diskimage
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAu0lEQVR42mP8//8/AymAEaihoaEBvyJkBVANuPQAxV1cXPbs2QNXQJQGIMPW1hbieMIaIIzGxkbsGtB0IlzCyIhFA5A8cNIXyDgYJbPVxRTI8JZ6DFWHqQEIVE6pzvmvgazBi+cxIx8DUA1ODUCh2O0xEGcs9lwC4eLU0FDcuO2LLMgle07bL3sCZKQw3sCpAeIHs/R5QNVANkTDwR0mOP0ADw3kUIInHJwacCYhijSgOQMXQGggRjUcAACHcMThthqWBgAAAABJRU5ErkJggg==",
					".iso", ".img", ".vmdk", ".vhd"
				},
				// electrical document
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAdklEQVR42mP8//8/AymAEaihoaEBvyJkBVANuPQAxV1cXPbs2QNXgNDg4Hn2wHZjIImswcF8M1ADkGFrawtxPGENEEZjYyO6BgJ+ZWRE1wAUwqUaqAaLBvJtAIqg+QToNwI2EKVhkNtAIJTwxABabEA1EKMaDgDm5qnhbYXBrwAAAABJRU5ErkJggg==",
					".pdf", ".xps", ".ps", ".dvi"
				},
				// executable
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAASklEQVR42mP8//8/AymAkRwNDQ0NxChtbGwEKoZqkIrKxqN0Wf4jIHlwhwkFGhITEwk6SUFBYVQDjTUAo52gBiCAaiBGKRyQrAEA0I194QBfsWoAAAAASUVORK5CYII=",
					".exe", ".bat", ".js", ".vbs", ".sh"
				},
				// image
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAjElEQVR42mP8//8/AymAEaihoaEBvyJkBVANuPQAxV1cXPbs2QNXQJQGIMPW1hbieIQGRkZGTA319fUQRmNjIxYNM2++wtSTri4GVAOUxa6h80YSRF25xjzCGpQ2+qAZD9RGmgYguOe/BaeGrc9kMTV4Sz0eQA24kgZ2DfiTE4oGPGaj2QPVQIxqOAAAiNe24ei+CfsAAAAASUVORK5CYII=",
					".bmp", ".png", ".jpg", ".jfif", ".jpeg", ".tif", ".tiff", ".gif", " .dicom", ".xbm", ".xpm", ".ppm", ".pgm", ".pbm", ".ico", ".svg", ".vml", ".wmf", ".emf", ".eps", ".psd", ".ai", ".hdr", ".exr", ".rgbe"
				},
				// markup
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAdElEQVR42q2R0Q3AIAhEe4P5516FvdyNkmDEWFqtKV+n3MuBQkSOLwUFiOjd1Bsq8MTofc65lNIMDgAY3NZSQHVKyYafJ5hg5hEIE3x0YClhDoAhpwQ6BHrHwASA97gu044q/kjY2WHnlZaA+5eF5cCKu9UF2Kat4Xr11NsAAAAASUVORK5CYII=",
					".htm", ".html", ".shtml", ".md", ".rtf"
				},
				// multimedia
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAbklEQVR42rWS0Q3AIAhEy2D8OZjuxW70Eiy1tlZsUr4OuBfQQKq6rQQByDm/m1pDBUYM6iklEXFDCIBgZlt+DpgopQwBIqq9Q1yKDjy378IATEQC3TnmQGf9AYiutPzoL986ubkWQBK50xOIuD12HVDU4SoPg2UAAAAASUVORK5CYII=",
					".wav", ".mid", ".midi", ".mp3", ".flac", ".mpg", "mpeg", ".mp4", ".wmv", ".wma", ".3gp"
				},
				// office
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAArUlEQVR42mP8//8/AymAEaihoaEBvyJkBVANuPQAxV1cXPbs2QNXgNDAyMh49Sq681atAmkAMmxtbSGOx6dBwl11UnI0hN3Y2IhFAwMDSOjqVajqFztvQ1RraTEAZXHaoK3N8FYGqhrIBurHpwFoMFBU+MltoDqIanw2vJVRgTsDqBruPAJOwgQ4NeCKOKAaLBoIpAhMDQ6eZ3GpPrDdmDIb8LgezSdQDcSohgMAeUqz4S+k07IAAAAASUVORK5CYII=",
					".xls", ".xlsx", ".xlsm", ".ppt", ".pptx", ".pptm", ".doc", ".docx", ".docm", ".ods", ".odp", ".odt",
				},
				// plane text
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAW0lEQVR42mP8//8/AymAEaihoaEBvyJkBVANuPQAxV1cXPbs2QNXQJQGIMPW1hbieIQGRkZGNNXIrm1sbETXQMCvjIyEbUC2CouGURuobAMes9HsgWogRjUcAACu0JXhKcCftQAAAABJRU5ErkJggg==",
					".txt", ".text"
				},
				// program source
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAf0lEQVR42mP8//8/AymAEaihoaEBvyJkBVANuPQAxV1cXPbs2QNXQJQGIMPW1hbieBQNDp5ngeSB7cYQ1UCug/lmCLuxsRG7BrhquB6ICCMjI04NEHsgVhGlgVgb4HLINsD1UMMG2vsBazzAudg14EtzyBqAHGLSKUIDMarhAADlYcPhd4fpHAAAAABJRU5ErkJggg==",
					".c", ".cc", ".cpp", ".cxx", ".cs", ".go", ".h", ".hpp", ".hxx", ".vb", ".java", ".tex", ".tcl", ".pl", ".py", ".rs", ".rb", ".asp", ".aspx", ".php", ".lua"
				},
				// runtime module
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAvUlEQVR42mP8//8/AymAEaihoaGBGKWNjY1AxVANUlHZBDWkq4tRoCExMZGgBgUFBZAGe48zcKGFM0SAZHzGGzQRdA1RE+WW5T9ClgPqwRRE14BmA1wQLo6iAciHS0NUQ0SQzULRsDw9LHLmKggJCZCZN1/B9UPchm4DUDVcBUQbPg0Q1c3NzUDS1tYWSAJtwOckiA0HDhyAGAlxD9xXB3eYQBgIDXBHQxgQEqjhwHZj9MSHHHGYAIsGUpM3AC7bsOFlNXRFAAAAAElFTkSuQmCC",
					".dll", ".so", ".class", ".sys"
				},
				// shortcut
				new[]{
					@"iVBORw0KGgoAAAANSUhEUgAAABAAAAAQCAIAAACQkWg2AAAAiUlEQVR42mP8//8/AymAEaihoaEBvyJkBVANuPQAxV1cXPbs2QNXgNDw4elGTA0TZp8HagAybG1tIY4nrAHCaGxsJEoDBAhI+zMyMpKrAX/4vH+yAYsGXFEBVIRTAyOGTUBZfBpgRpKiAc0SAhrgqiGWAblEaYB7Hq4BybV4NcAVoTqSiHjADHEAVeaf4RiLmTwAAAAASUVORK5CYII=",
					".lnk"
				},
			};
			_extensionToImageListIndex = new Dictionary<string, int>();
			foreach (var iconInfo in iconsInfo) {
				var curIndex = imageList.Images.Count;
				for (var i = 1; i < iconInfo.Length; ++i) {
					_extensionToImageListIndex.Add(iconInfo[i], curIndex);
				}
				imageList.Images.Add(CreateBitmapFromBase64(iconInfo[0]));
			}

			listViewFileList.Clear();
			listViewFileList.SmallImageList = imageList;
			listViewFileList.Columns.Add("Name", 400);
			listViewFileList.Columns.Add("Date modified", 150);
			listViewFileList.Columns.Add("File size", 100);
			listViewFileList.Columns[2].TextAlign = HorizontalAlignment.Right;
			listViewFileList.View = View.Details;
			listViewFileList.MultiSelect = Multiselect;

			comboBoxFilter.DataSource = null;
			if (Filter != null) {
				var filters = Enumerable.Range(0, 0).Select(v => new { Display = "", Value = "" }).ToList();
				var filtersStr = Filter.Split('|');
				for (var i = 0; i < filtersStr.Length - 1; i += 2) {
					filters.Add(new { Display = filtersStr[i], Value = filtersStr[i + 1] });
				}
				if (filters.Count > 0) {
					comboBoxFilter.DisplayMember = "Display";
					comboBoxFilter.ValueMember = "Value";
					comboBoxFilter.DataSource = filters.ToArray();
					var filterIndex = FilterIndex - 1;
					if (filterIndex < 0) filterIndex = 0;
					if (filterIndex < filters.Count) comboBoxFilter.SelectedIndex = filterIndex;
				}
			}

			_customFilterString = "";

			// initialize undo&redo buffer
			_undoIndex = -1;
			_undoRedoBuffer = new List<string>();

			if (Directory.Exists(InitialDirectory)) {
				CurrentDirectory = InitialDirectory;
			} else {
				CurrentDirectory = System.IO.Directory.GetCurrentDirectory();
			}
			if (_fileDialogMode != FileDialogMode.SelectFolder) {
				textBoxFileName.Text = FileName;
			}
		}

		private void RedrawListView() {
			if (!Visible || !Directory.Exists(_currentDirectory)) {
				return;
			}

			var cursor = Cursor.Current;
			try {

				Cursor.Current = Cursors.WaitCursor;

				var items = new List<ListViewItem>();
				// display folders and files.

				var childDirs = Directory.GetDirectories(_currentDirectory).Select(dir=>Path.GetFileName(dir));
				IEnumerable<string> childFiles;
				if (!string.IsNullOrEmpty(_customFilterString)) {
					var filter = _customFilterString;
					if (filter.Length == 0 || filter[filter.Length - 1] != '.') filter += '*';
					childFiles = Directory.GetFiles(_currentDirectory, filter).Select(file=>Path.GetFileName(file));
				} else {
					var filters = comboBoxFilter.Items.Count > 0 ? ((string)comboBoxFilter.SelectedValue).Split(';') : new string[]{};
					childFiles = filters.SelectMany(filter => Directory.GetFiles(_currentDirectory, filter)).Select(file=>Path.GetFileName(file));
				}

				_children = 
					childDirs.Select(dir=> new ChildItem(true, dir, "", "", 0)).Concat(
					childFiles.Select(file=>{
						var ext = Path.GetExtension(file).ToLower();
						int idx;
						if (!_extensionToImageListIndex.TryGetValue(ext, out idx)) {
							idx = 1;
						}
						return new ChildItem(false, file, "", "", idx);
					})
				).ToArray();

				textBoxTargetFolder.Text = CurrentDirectory;
				textBoxTargetFolder.SelectionStart = textBoxTargetFolder.Text.Length - 1;
				textBoxTargetFolder.SelectionLength = 0;

				listViewFileList.VirtualListSize = _children.Length;
				listViewFileList.Invalidate();
			} finally {
				Cursor.Current = cursor;
			}
		}

		private bool CheckValid(string path, bool withWildcard) {
			if (string.IsNullOrEmpty(path)) return true;
			var origInvalPathCharsWithoutDQ = Path.GetInvalidPathChars().Where(c=> c != '"');
			var invalidPathChars = withWildcard ?
				origInvalPathCharsWithoutDQ.Concat(new[] { '*', '?' }).ToArray() : origInvalPathCharsWithoutDQ.ToArray();
			if (path.IndexOfAny(invalidPathChars) >= 0
				|| Regex.IsMatch(path, @"(^|\\|/)(CON|PRN|AUX|NUL|CLOCK\$|COM[0-9]|LPT[0-9])(\.|\\|/|$|\t)", RegexOptions.IgnoreCase)) {
				return false;
			}
			return true;
		}

		private Bitmap CreateBitmapFromBase64(string base64) {
			using (var ms = new MemoryStream(System.Convert.FromBase64String(base64), false)) {
				ms.Position = 0;
				return new Bitmap(ms);
			}
		}

		// callbacks
		private void radioButtonList_CheckedChanged(object sender, EventArgs e) {
			listViewFileList.View = View.List;
		}

		private void radioButtonDetails_CheckedChanged(object sender, EventArgs e) {
			listViewFileList.View = View.Details;
		}

		private void buttonRedraw_Click(object sender, EventArgs e) {
			RedrawListView();
		}

		private void buttonOK_Click(object sender, EventArgs e)
		{
			DialogResult = System.Windows.Forms.DialogResult.OK;
			Close();
		}

		private void buttonCancel_Click(object sender, EventArgs e)
		{
			DialogResult = System.Windows.Forms.DialogResult.Cancel;
			Close();
		}

		// abc.txt  => string[]{"abc.txt"}
		// "abc.txt" "def.txt" "efg.txt"    => string[]{"abc.txt", "def.txt", "efg.txt"}
		// abc.txt "def.txt" "efg.txt"      => null   (error)
		// "abc.txt" def.txt "efg.txt"      => string[]{"abc.txt", "def.txt", "efg.txt"}
		// "abc.txt" " "def.txt" "efg.txt"  => string[]{"abc.txt", " ", "def.txt", "efg.txt"}
		// "*.txt" " "def.txt" "efg.txt"    => null   (error)
		// ".txt" " "def.txt" "efg.txt"     => null   (error)
		// "" => string[]{""}
		private string[] ParseFileNames(string text){
			var fileNames = new List<string>();
			var idxFirstQuote = text.IndexOf('"');
			if (idxFirstQuote >= 0) {
				if (text.Substring(0, idxFirstQuote).Trim() != "") return null;
				int idxOpenQuote = -1;
				for (var i = 0; i < text.Length; ++i){
					char c = text[i];
					if (c == '"'){
						if (idxOpenQuote == -1){
							idxOpenQuote = i;
						}else{
							var fileNameToAdd = text.Substring(idxOpenQuote + 1, i - idxOpenQuote - 1);
							if (!CheckValid(fileNameToAdd, true)) return null;
							fileNames.Add(fileNameToAdd);
							idxOpenQuote = -1;
						}
					} else if (idxOpenQuote == -1 && c != ' ') {
						idxOpenQuote = i - 1;
					}
				}
				if (idxOpenQuote >= 0) fileNames.Add(text.Substring(idxOpenQuote + 1).TrimEnd());
				return fileNames.ToArray();
			}else{
				return new string[]{text.Trim()};
			}
		}

		private void SimpleFileDialog_FormClosing(object sender, FormClosingEventArgs e) {
			if (DialogResult == System.Windows.Forms.DialogResult.OK){
				e.Cancel = true;
				if (_fileDialogMode == FileDialogMode.SelectFolder){
					FileName = CurrentDirectory;
					FileNames = new string[1]{FileName};
					e.Cancel = false;
					return;
				}
				var target = textBoxFileName.Text;
				if (string.IsNullOrEmpty(target)) return;

				var rawFileNames = ParseFileNames(target);
				if (rawFileNames == null) {
					MessageBox.Show(this, "invalid name", "error", MessageBoxButtons.OK);
					return;
				}

				int cnt = Multiselect ? rawFileNames.Length : 1;

				if (cnt == 1) {
					var fullPath = Path.IsPathRooted(rawFileNames[0]) ? rawFileNames[0] : Path.Combine(_currentDirectory, rawFileNames[0]);
					var fileName = Path.GetFileName(rawFileNames[0]);
					if (Directory.Exists(fullPath)) {
						// change directory
						CurrentDirectory = fullPath;
						textBoxFileName.Text = "";
						return;
					} else if (fileName.IndexOfAny(new char[] { '?', '*' }) >= 0) {
						// custom filter
						textBoxFileName.Text = fileName;
						textBoxFileName.SelectAll();
						_customFilterString = fileName;
						RedrawListView();
						return;
					} else if (OverwritePrompt && File.Exists(fullPath)) {
						// overwrite prompt for SaveFile mode
						var rc = MessageBox.Show(this, fileName + " exists. override?", "confirm to override", MessageBoxButtons.YesNo);
						if (rc == System.Windows.Forms.DialogResult.No) return;
					}
				}

//				FileName = "";
//				FileNames = new string[1] { "" };
				
				var fileNamesToWrite = new string[cnt];
				for (var i = 0; i < cnt; ++i){
					var fullPath = Path.IsPathRooted(rawFileNames[i]) ? rawFileNames[i] : Path.Combine(_currentDirectory, rawFileNames[i]);
					var fileName = Path.GetFileName(fullPath);

					if (CheckFileExists && !File.Exists(fullPath)) {
						MessageBox.Show(this, fullPath + " not found.", "file not found", MessageBoxButtons.OK);
						return;
					}
					if (!Path.HasExtension(fullPath) && !string.IsNullOrEmpty(DefaultExt)) {
						fullPath += "." + DefaultExt;
					}
					fileNamesToWrite[i] = fullPath;
				}
				FileName = fileNamesToWrite[0];
				FileNames = fileNamesToWrite;
				e.Cancel = false;
			}
			FilterIndex = comboBoxFilter.SelectedIndex + 1;
		}

		private void buttonUp_Click(object sender, EventArgs e) {
			var parentDir = Path.GetDirectoryName(CurrentDirectory);
			if (!Directory.Exists(parentDir)) return;

			CurrentDirectory = parentDir;
		}

		private void textBoxTargetFolder_KeyPress(object sender, KeyPressEventArgs e) {
			if (e.KeyChar == (char)Keys.Enter) {
				var newDir = textBoxTargetFolder.Text;
				if (Directory.Exists(newDir)) {
					CurrentDirectory = newDir;
				} else {
					textBoxTargetFolder.Text = CurrentDirectory;
				}
				e.Handled = true;
			}
		}

		private void textBoxFileName_KeyPress(object sender, KeyPressEventArgs e) {
			if (e.KeyChar == (char)Keys.Enter) {
				DialogResult = System.Windows.Forms.DialogResult.OK;
				Close();
				e.Handled = true;
			}
		}

		private void listViewFileList_RetrieveVirtualItem(object sender, RetrieveVirtualItemEventArgs e) {
			if (e.ItemIndex < 0 || e.ItemIndex >= _children.Length) return;
			if (e.Item == null) e.Item = new ListViewItem(new string[]{"", "", ""});
			var item = _children[e.ItemIndex];
			e.Item.Text = item.text;
			e.Item.ImageIndex = item.imageIndex;
			if (listViewFileList.View == View.Details){
				if (item.isDir){
					e.Item.SubItems[1].Text = "";
					e.Item.SubItems[2].Text = "";
				}else{
					if (string.IsNullOrEmpty(item.dateModified) || string.IsNullOrEmpty(item.fileSize)) {
						for (var j = e.ItemIndex; j < e.ItemIndex + 50 && j < _children.Length; ++j) {
							var file = _children[j].text;
							var unitname = new string[] { " KB", " MB", " GB", " TB" };
							var fi = new FileInfo(Path.Combine(CurrentDirectory, file));

							var fileSize = fi.Length;
							string fileSizeStr = "> 10 TB";

							long denom = 1024L;
							for (var i = 0; i < 4; ++i) {
								if (fileSize < denom * 10240L) {
									long reminder;
									long quot = Math.DivRem(fileSize, denom, out reminder);
									fileSizeStr = (quot + (reminder > 0L ? 1L : 0L)).ToString() + unitname[i];
									break;
								}
								denom *= 1024L;
							}
							_children[j].dateModified = fi.LastWriteTime.ToString();
							_children[j].fileSize = fileSizeStr;
						}
					}

					e.Item.SubItems[1].Text = item.dateModified;
					e.Item.SubItems[2].Text = item.fileSize;
				}
			}
		}

		private void listViewFileList_Click(object sender, EventArgs e) {
			if (listViewFileList.SelectedIndices.Count == 0) return;
			var fileList = "";
			int listCount = 0;
			for (var i = 0; i < listViewFileList.SelectedIndices.Count; ++i){
				var itmIndex = listViewFileList.SelectedIndices[i];
				if (itmIndex < 0 || itmIndex >= _children.Length) continue;
				var fullPath = Path.Combine(CurrentDirectory, _children[itmIndex].text);

				if (Directory.Exists(fullPath)) {
					continue;
				} else {
					if (!string.IsNullOrEmpty(fileList)) fileList += ' ';
					fileList += '"' + _children[itmIndex].text + '"';
					++listCount;
				}
			}
			if (!string.IsNullOrEmpty(fileList)){
				textBoxFileName.Text = (listCount == 1) ? fileList.Substring(1, fileList.Length - 2) : fileList;
			}
		}

		private void listViewFileList_DoubleClick(object sender, EventArgs e) {
			if (listViewFileList.SelectedIndices.Count == 0) return;
			var itmIndex = listViewFileList.SelectedIndices[0];
			if (itmIndex < 0 || itmIndex >= _children.Length) return;
			var fullPath = Path.Combine(CurrentDirectory, _children[itmIndex].text);

			if (Directory.Exists(fullPath)) {
				if (string.IsNullOrEmpty(_customFilterString)) textBoxFileName.Text = "";
				CurrentDirectory = fullPath;
			} else {
				DialogResult = System.Windows.Forms.DialogResult.OK;
				Close();
			}
		}

		private void comboBoxFilter_SelectedValueChanged(object sender, EventArgs e) {
			_customFilterString = "";
			RedrawListView();
		}

		private void buttonUndo_Click(object sender, EventArgs e) {
			if (_undoIndex <= 0 && _undoIndex >= _undoRedoBuffer.Count - 1) return;
			--_undoIndex;
			if (_undoIndex < _undoRedoBuffer.Count - 1) buttonRedo.Enabled = true;
			if (_undoIndex == 0) buttonUndo.Enabled = false;
			_currentDirectory = _undoRedoBuffer[_undoIndex];
			RedrawListView();
		}

		private void buttonRedo_Click(object sender, EventArgs e) {
			if (_undoIndex <= 0 && _undoIndex >= _undoRedoBuffer.Count - 1) return;
			++_undoIndex;
			if (_undoIndex == _undoRedoBuffer.Count - 1) buttonRedo.Enabled = false;
			buttonUndo.Enabled = true;
			_currentDirectory = _undoRedoBuffer[_undoIndex];
			RedrawListView();
		}

		private void textBoxTargetFolder_Enter(object sender, EventArgs e) {
			var tm = new Timer();
			tm.Interval = 1;
			tm.Tick += (s_, e_) => {
				textBoxTargetFolder.SelectAll();
				tm.Stop();
				tm.Dispose();
			};
			tm.Start();
		}

	}
}
