class ProcessManager {
  constructor() {
    this.jobTable = [];  // Array of {jobId, pid, command, status, windowRef, terminalElement}
    this.nextJobId = 1;
    this.foregroundJob = null;  // Currently running foreground job
    this.activeTerminal = null; // Currently active terminal
  }
  
  /**
   * Start a new process
   * @param {string} command - The command that was executed
   * @param {Array} args - Arguments passed to the command
   * @param {HTMLElement} terminalElement - The terminal element that initiated the command
   * @param {boolean} isBackground - Whether the process should run in background
   * @returns {Object} - Object containing jobId, pid, and isForeground status
   */
  startProcess(command, args, terminalElement, isBackground = false) {
    const jobId = this.nextJobId++;
    const pid = Date.now() + Math.random();  // Unique process identifier
    
    const process = {
      jobId: jobId,
      pid: pid,
      command: command,
      args: args,
      status: isBackground ? 'RUNNING' : 'RUNNING_FG',
      startTime: new Date(),
      terminalElement: terminalElement,
      windowRef: null,  // Will be set when app creates window
      isBackground: isBackground
    };
    
    this.jobTable.push(process);
    
    if (!isBackground) {
      this.foregroundJob = process;
      // Get the actual terminal window element from the terminalElement
      this.activeTerminal = terminalElement.closest('.terminal-window') || terminalElement;
    }
    
    console.log(`Started process: ${command} with jobId: ${jobId}, pid: ${pid}, background: ${isBackground}`);
    
    return { jobId, pid, isForeground: !isBackground };
  }
  
  /**
   * Register an app window with a process
   * @param {number} jobId - The job ID to register the window with
   * @param {Object} windowRef - Reference to the window object or app instance
   * @returns {boolean} - True if registration was successful
   */
  registerAppWindow(jobId, windowRef) {
    const job = this.jobTable.find(j => j.jobId === jobId);
    if (job) {
      // Update the windowRef - it can be an app instance or DOM element
      job.windowRef = windowRef;
      console.log(`Registered window for jobId: ${jobId}`);
      return true;
    }
    console.log(`Failed to register window - jobId ${jobId} not found`);
    return false;
  }
  
  /**
   * Terminate a process
   * @param {number} jobId - The job ID to terminate
   * @returns {boolean} - True if termination was successful
   */
  terminateProcess(jobId) {
    const jobIndex = this.jobTable.findIndex(j => j.jobId === jobId);
    if (jobIndex !== -1) {
      const job = this.jobTable[jobIndex];
      
      console.log(`Terminating process: ${job.command} (jobId: ${jobId})`);
      
      // Close the associated window if it exists
      if (job.windowRef) {
        // Check if windowRef is an app instance with closeGUI
        if (job.windowRef.closeGUI && typeof job.windowRef.closeGUI === 'function') {
          // This is an app instance, call its closeGUI method for proper cleanup
          // Pass false to prevent it from notifying the ProcessManager (avoiding circular calls)
          job.windowRef.closeGUI(false);
        } 
        // Check if windowRef is a DOM element
        else if (job.windowRef.remove && typeof job.windowRef.remove === 'function') {
          // This is a DOM element, try to find the associated app instance to ensure proper cleanup
          // The DOM element ID should contain the instance ID in the format "${instanceId}-main-window"
          if (job.windowRef.id) {
            const idParts = job.windowRef.id.split('-main-window');
            if (idParts.length > 0) {
              const potentialInstanceId = idParts[0];
              if (window.appInstances && window.appInstances[potentialInstanceId]) {
                // Found the app instance, call its closeGUI for proper cleanup but don't notify PM
                window.appInstances[potentialInstanceId].closeGUI(false);
              } else {
                // If we can't find the app instance, just remove the DOM element
                if (job.windowRef.parentElement) {
                  job.windowRef.parentElement.removeChild(job.windowRef);
                } else {
                  job.windowRef.remove();
                }
              }
            } else {
              // Just remove the DOM element directly
              if (job.windowRef.parentElement) {
                job.windowRef.parentElement.removeChild(job.windowRef);
              } else {
                job.windowRef.remove();
              }
            }
          } else {
            // Just remove the DOM element directly
            if (job.windowRef.parentElement) {
              job.windowRef.parentElement.removeChild(job.windowRef);
            } else {
              job.windowRef.remove();
            }
          }
        } else {
          // Fallback for other types of references
          if (typeof job.windowRef.remove === 'function') {
            job.windowRef.remove();
          } else if (job.windowRef.parentElement) {
            job.windowRef.parentElement.removeChild(job.windowRef);
          }
        }
      }
      
      // Remove from job table
      this.jobTable.splice(jobIndex, 1);
      
      // If it was the foreground job, clear the foreground job
      if (this.foregroundJob && this.foregroundJob.jobId === jobId) {
        this.foregroundJob = null;
        this.activeTerminal = null;
      }
      
      return true;
    }
    return false;
  }
  
  /**
   * Suspend a process
   * @param {number} jobId - The job ID to suspend
   * @returns {boolean} - True if suspension was successful
   */
  suspendProcess(jobId) {
    const job = this.jobTable.find(j => j.jobId === jobId);
    if (job) {
      job.status = 'STOPPED';
      if (this.foregroundJob && this.foregroundJob.jobId === jobId) {
        this.foregroundJob = null;
        this.activeTerminal = null;
      }
      console.log(`Suspended process: ${job.command} (jobId: ${jobId})`);
      return true;
    }
    return false;
  }
  
  /**
   * Resume a suspended process
   * @param {number} jobId - The job ID to resume
   * @param {boolean} asBackground - Whether to resume as background process
   * @returns {boolean} - True if resumption was successful
   */
  resumeProcess(jobId, asBackground = false) {
    const job = this.jobTable.find(j => j.jobId === jobId);
    if (job) {
      job.status = asBackground ? 'RUNNING' : 'RUNNING_FG';
      if (!asBackground) {
        this.foregroundJob = job;
        this.activeTerminal = job.terminalElement;
      }
      console.log(`Resumed process: ${job.command} (jobId: ${jobId}), background: ${asBackground}`);
      return true;
    }
    return false;
  }
  
  /**
   * List all jobs
   * @returns {Array} - Array of job objects with jobId, command, status, and startTime
   */
  listJobs() {
    return this.jobTable.map(job => ({
      jobId: job.jobId,
      command: job.command,
      status: job.status,
      startTime: job.startTime
    }));
  }
  
  /**
   * Get job by ID
   * @param {number} jobId - The job ID to retrieve
   * @returns {Object} - The job object or null if not found
   */
  getJobById(jobId) {
    return this.jobTable.find(j => j.jobId === jobId);
  }
  
  /**
   * Handle window close event
   * @param {number} jobId - The job ID to handle
   * @returns {boolean} - True if successful
   */
  handleWindowClose(jobId) {
    console.log(`Handling window close for jobId: ${jobId}`);
    // Called when a window is closed externally
    // Just remove the job from the job table, don't close window again
    const jobIndex = this.jobTable.findIndex(j => j.jobId === jobId);
    if (jobIndex !== -1) {
      // Remove from job table
      this.jobTable.splice(jobIndex, 1);
      
      // If it was the foreground job, clear the foreground job
      if (this.foregroundJob && this.foregroundJob.jobId === jobId) {
        this.foregroundJob = null;
        this.activeTerminal = null;
      }
      
      return true;
    }
    return false;
  }
  
  /**
   * Get the current foreground job
   * @returns {Object} - The foreground job or null
   */
  getForegroundJob() {
    return this.foregroundJob;
  }
  
  /**
   * Check if a specific job is running in foreground
   * @param {number} jobId - The job ID to check
   * @returns {boolean} - True if job is running in foreground
   */
  isForegroundJob(jobId) {
    return this.foregroundJob && this.foregroundJob.jobId === jobId;
  }
  
  /**
   * Get active terminal element
   * @returns {HTMLElement} - The active terminal element
   */
  getActiveTerminal() {
    return this.activeTerminal;
  }
  
  /**
   * Clear foreground job (used when terminal regains control)
   */
  clearForegroundJob() {
    this.foregroundJob = null;
    this.activeTerminal = null;
  }
}

// Global process manager instance
if (typeof window !== 'undefined') {
  window.processManager = new ProcessManager();
}