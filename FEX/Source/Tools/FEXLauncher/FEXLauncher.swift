import AppKit
import Foundation

class LauncherDelegate: NSObject, NSApplicationDelegate, NSTableViewDelegate, NSTableViewDataSource {
    var window: NSWindow!
    
    // UI Elements
    var selectButton: NSButton!
    var launchButton: NSButton!
    var stopButton: NSButton!
    var pathField: NSTextField!
    var logView: NSTextView!
    var statusLabel: NSTextField!
    var statusIndicator: NSView!
    var recentTableView: NSTableView!
    var clearRecentsButton: NSButton!
    
    // Process and Data
    var process: Process?
    var outputPipe: Pipe?
    var selectedExePath: String?
    var recents: [String] = []
    
    func applicationDidFinishLaunching(_ notification: Notification) {
        // Create main window
        let frame = NSRect(x: 0, y: 0, width: 850, height: 600)
        window = NSWindow(contentRect: frame,
                          styleMask: [.titled, .closable, .miniaturizable, .resizable],
                          backing: .buffered,
                          defer: false)
        window.title = "FEX JIT Emulation Launcher"
        window.center()
        
        // Translucent glassmorphic background
        let visualEffectView = NSVisualEffectView(frame: frame)
        visualEffectView.material = .hudWindow
        visualEffectView.blendingMode = .behindWindow
        visualEffectView.state = .active
        visualEffectView.autoresizingMask = [.width, .height]
        window.contentView = visualEffectView
        
        setupSidebar(in: visualEffectView)
        setupMainArea(in: visualEffectView)
        
        loadRecents()
        updateUI()
        
        window.makeKeyAndOrderFront(nil)
        NSApp.activate(ignoringOtherApps: true)
    }
    
    func setupSidebar(in container: NSView) {
        let sidebarWidth: CGFloat = 220
        let padding: CGFloat = 15
        
        // Title
        let titleLabel = NSTextField(labelWithString: "Recent Applications")
        titleLabel.frame = NSRect(x: padding, y: 600 - 40, width: sidebarWidth - padding, height: 20)
        titleLabel.font = NSFont.boldSystemFont(ofSize: 13)
        titleLabel.textColor = .secondaryLabelColor
        container.addSubview(titleLabel)
        
        // Table View for Recents list
        let tableScrollView = NSScrollView(frame: NSRect(x: padding, y: 60, width: sidebarWidth - padding, height: 600 - 110))
        tableScrollView.hasVerticalScroller = true
        tableScrollView.drawsBackground = false
        
        recentTableView = NSTableView(frame: tableScrollView.bounds)
        let column = NSTableColumn(identifier: NSUserInterfaceItemIdentifier("RecentCol"))
        column.width = sidebarWidth - padding - 20
        column.title = ""
        recentTableView.addTableColumn(column)
        recentTableView.headerView = nil
        recentTableView.delegate = self
        recentTableView.dataSource = self
        recentTableView.backgroundColor = .clear
        recentTableView.autoresizingMask = [.width, .height]
        
        tableScrollView.documentView = recentTableView
        container.addSubview(tableScrollView)
        
        // Clear recents button
        clearRecentsButton = NSButton(title: "Clear Recents", target: self, action: #selector(clearRecents))
        clearRecentsButton.frame = NSRect(x: padding, y: 15, width: sidebarWidth - padding, height: 25)
        clearRecentsButton.bezelStyle = .rounded
        container.addSubview(clearRecentsButton)
    }
    
    func setupMainArea(in container: NSView) {
        let startX: CGFloat = 240
        let width = 850 - startX - 15
        
        // Top Banner / Header
        let headerView = NSView(frame: NSRect(x: startX, y: 600 - 90, width: width, height: 75))
        headerView.wantsLayer = true
        headerView.layer?.backgroundColor = NSColor.controlBackgroundColor.withAlphaComponent(0.4).cgColor
        headerView.layer?.cornerRadius = 10
        headerView.autoresizingMask = [.width]
        container.addSubview(headerView)
        
        let headerTitle = NSTextField(labelWithString: "FEX JIT Emulation Launcher")
        headerTitle.frame = NSRect(x: 15, y: 40, width: width - 150, height: 25)
        headerTitle.font = NSFont.boldSystemFont(ofSize: 16)
        headerView.addSubview(headerTitle)
        
        let headerDesc = NSTextField(labelWithString: "Run Windows x86_64 executables natively on Apple Silicon")
        headerDesc.frame = NSRect(x: 15, y: 15, width: width - 150, height: 18)
        headerDesc.font = NSFont.systemFont(ofSize: 11)
        headerDesc.textColor = .secondaryLabelColor
        headerView.addSubview(headerDesc)
        
        // Status indicator in header
        statusIndicator = NSView(frame: NSRect(x: width - 120, y: 27, width: 10, height: 10))
        statusIndicator.wantsLayer = true
        statusIndicator.layer?.cornerRadius = 5
        statusIndicator.layer?.backgroundColor = NSColor.gray.cgColor
        headerView.addSubview(statusIndicator)
        
        statusLabel = NSTextField(labelWithString: "IDLE")
        statusLabel.frame = NSRect(x: width - 105, y: 22, width: 90, height: 20)
        statusLabel.font = NSFont.boldSystemFont(ofSize: 11)
        statusLabel.textColor = .secondaryLabelColor
        headerView.addSubview(statusLabel)
        
        // Path input block
        let pathLabel = NSTextField(labelWithString: "Windows Executable Path (.exe)")
        pathLabel.frame = NSRect(x: startX, y: 600 - 135, width: width, height: 20)
        pathLabel.font = NSFont.boldSystemFont(ofSize: 12)
        container.addSubview(pathLabel)
        
        pathField = NSTextField(frame: NSRect(x: startX, y: 600 - 165, width: width - 100, height: 22))
        pathField.placeholderString = "Select a Windows .exe file to begin..."
        pathField.font = NSFont.userFixedPitchFont(ofSize: 12)
        pathField.autoresizingMask = [.width]
        container.addSubview(pathField)
        
        selectButton = NSButton(title: "Browse...", target: self, action: #selector(selectExe))
        selectButton.frame = NSRect(x: 850 - 105, y: 600 - 167, width: 90, height: 25)
        selectButton.bezelStyle = .rounded
        selectButton.autoresizingMask = [.minXMargin]
        container.addSubview(selectButton)
        
        // Run/Stop buttons
        launchButton = NSButton(title: "Launch Application", target: self, action: #selector(launch))
        launchButton.frame = NSRect(x: startX, y: 600 - 210, width: (width / 2) - 10, height: 32)
        launchButton.bezelStyle = .rounded
        launchButton.autoresizingMask = [.width, .maxXMargin]
        launchButton.bezelColor = .systemBlue
        container.addSubview(launchButton)
        
        stopButton = NSButton(title: "Stop Process", target: self, action: #selector(terminateProcess))
        stopButton.frame = NSRect(x: startX + (width / 2) + 10, y: 600 - 210, width: (width / 2) - 15, height: 32)
        stopButton.bezelStyle = .rounded
        stopButton.autoresizingMask = [.width, .minXMargin]
        container.addSubview(stopButton)
        
        // Logs Panel
        let logLabel = NSTextField(labelWithString: "Live Output Logs")
        logLabel.frame = NSRect(x: startX, y: 600 - 250, width: width - 100, height: 20)
        logLabel.font = NSFont.boldSystemFont(ofSize: 12)
        container.addSubview(logLabel)
        
        let clearLogsButton = NSButton(title: "Clear Logs", target: self, action: #selector(clearLogs))
        clearLogsButton.frame = NSRect(x: 850 - 105, y: 600 - 252, width: 90, height: 25)
        clearLogsButton.bezelStyle = .rounded
        clearLogsButton.autoresizingMask = [.minXMargin]
        container.addSubview(clearLogsButton)
        
        let logScrollView = NSScrollView(frame: NSRect(x: startX, y: 20, width: width, height: 600 - 280))
        logScrollView.hasVerticalScroller = true
        logScrollView.autoresizingMask = [.width, .height]
        
        logView = NSTextView(frame: logScrollView.bounds)
        logView.isEditable = false
        logView.backgroundColor = .black
        logView.textColor = .green
        logView.font = NSFont.userFixedPitchFont(ofSize: 12)
        logView.autoresizingMask = [.width]
        
        logScrollView.documentView = logView
        container.addSubview(logScrollView)
    }
    
    // MARK: - Actions
    @objc func selectExe() {
        let openPanel = NSOpenPanel()
        openPanel.allowedFileTypes = ["exe"]
        openPanel.allowsMultipleSelection = false
        openPanel.canChooseDirectories = false
        openPanel.canChooseFiles = true
        openPanel.title = "Select Windows Executable (.exe)"
        
        if openPanel.runModal() == .OK {
            if let path = openPanel.url?.path {
                self.selectedExePath = path
                self.pathField.stringValue = path
                addToRecents(path: path)
                updateUI()
            }
        }
    }
    
    @objc func launch() {
        guard let exePath = selectedExePath ?? (pathField.stringValue.isEmpty ? nil : pathField.stringValue) else { return }
        
        logView.string = "[Launcher] Preparing environment...\n"
        
        let fexBinPath = "/Users/firataktug/Desktop/FEX/build/Bin/FEXMacOS"
        if !FileManager.default.fileExists(atPath: fexBinPath) {
            logView.string += "[Error] FEXMacOS binary not found at: \(fexBinPath)\n"
            logView.string += "Please run 'ninja' in the build directory first.\n"
            return
        }
        
        let newProcess = Process()
        newProcess.executableURL = URL(fileURLWithPath: fexBinPath)
        newProcess.arguments = [exePath]
        newProcess.currentDirectoryURL = URL(fileURLWithPath: "/Users/firataktug/Desktop/FEX")
        
        let pipe = Pipe()
        newProcess.standardOutput = pipe
        newProcess.standardError = pipe
        
        self.outputPipe = pipe
        self.process = newProcess
        
        statusIndicator.layer?.backgroundColor = NSColor.systemGreen.cgColor
        statusLabel.stringValue = "RUNNING"
        statusLabel.textColor = .systemGreen
        
        let fileHandle = pipe.fileHandleForReading
        fileHandle.readabilityHandler = { [weak self] handle in
            let data = handle.availableData
            if data.isEmpty { return }
            if let str = String(data: data, encoding: .utf8) {
                DispatchQueue.main.async {
                    self?.logView.string += str
                    self?.logView.scrollToEndOfDocument(nil)
                }
            }
        }
        
        newProcess.terminationHandler = { [weak self] proc in
            DispatchQueue.main.async {
                self?.statusIndicator.layer?.backgroundColor = NSColor.gray.cgColor
                self?.statusLabel.stringValue = "IDLE"
                self?.statusLabel.textColor = .secondaryLabelColor
                self?.logView.string += "\n[Launcher] Process exited with status code \(proc.terminationStatus)\n"
                self?.logView.scrollToEndOfDocument(nil)
                self?.process = nil
                self?.outputPipe = nil
                self?.updateUI()
            }
        }
        
        do {
            try newProcess.run()
            logView.string += "[Launcher] Successfully launched: \(exePath)\n\n"
            updateUI()
        } catch {
            logView.string += "[Error] Failed to start process: \(error.localizedDescription)\n"
            statusIndicator.layer?.backgroundColor = NSColor.gray.cgColor
            statusLabel.stringValue = "IDLE"
            statusLabel.textColor = .secondaryLabelColor
            self.process = nil
            self.outputPipe = nil
            updateUI()
        }
    }
    
    @objc func terminateProcess() {
        if let process = self.process, process.isRunning {
            logView.string += "\n[Launcher] Terminating process manually...\n"
            process.terminate()
        }
    }
    
    @objc func clearLogs() {
        logView.string = ""
    }
    
    @objc func clearRecents() {
        UserDefaults.standard.removeObject(forKey: "FEXRecents")
        self.recents = []
        recentTableView.reloadData()
    }
    
    // MARK: - Recents Management
    func addToRecents(path: String) {
        var list = UserDefaults.standard.stringArray(forKey: "FEXRecents") ?? []
        if let index = list.firstIndex(of: path) {
            list.remove(at: index)
        }
        list.insert(path, at: 0)
        if list.count > 10 {
            list = Array(list.prefix(10))
        }
        UserDefaults.standard.set(list, forKey: "FEXRecents")
        self.recents = list
        recentTableView.reloadData()
    }
    
    func loadRecents() {
        self.recents = UserDefaults.standard.stringArray(forKey: "FEXRecents") ?? []
        recentTableView.reloadData()
    }
    
    func updateUI() {
        let isRunning = process?.isRunning ?? false
        selectButton.isEnabled = !isRunning
        pathField.isEnabled = !isRunning
        launchButton.isEnabled = !isRunning && (selectedExePath != nil || !pathField.stringValue.isEmpty)
        stopButton.isEnabled = isRunning
    }
    
    // MARK: - Table View Delegates
    func numberOfRows(in tableView: NSTableView) -> Int {
        return recents.count
    }
    
    func tableView(_ tableView: NSTableView, viewFor tableColumn: NSTableColumn?, row: Int) -> NSView? {
        let path = recents[row]
        let fileName = URL(fileURLWithPath: path).lastPathComponent
        
        let identifier = NSUserInterfaceItemIdentifier("RecentCell")
        var textField = tableView.makeView(withIdentifier: identifier, owner: self) as? NSTextField
        if textField == nil {
            textField = NSTextField(labelWithString: "")
            textField?.identifier = identifier
            textField?.font = NSFont.userFixedPitchFont(ofSize: 11)
        }
        textField?.stringValue = fileName
        return textField
    }
    
    func tableViewSelectionDidChange(_ notification: Notification) {
        let selectedRow = recentTableView.selectedRow
        if selectedRow >= 0 && selectedRow < recents.count {
            let path = recents[selectedRow]
            self.selectedExePath = path
            self.pathField.stringValue = path
            updateUI()
        }
    }
}

// Global entry point
let app = NSApplication.shared
let delegate = LauncherDelegate()
app.delegate = delegate
app.setActivationPolicy(.regular)
app.run()
