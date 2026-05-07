---
inclusion: always
---

# Task Execution Protocol - MANDATORY

This protocol defines **six mandatory steps** that MUST be followed when executing **each phase**. No shortcuts. No exceptions.

---

## Step 1: Pre-Git Requirement (Before Task Execution)

**Purpose:** Ensure complete and accurate Git branch information to prevent issues during new branch creation and checkout.

**Actions:**

1. **Check Current State:**
   - Execute `git status` to check current branch name, uncommitted changes, untracked files
   - Execute `git branch -vv` to display all local branches and tracking information

2. **Update Remote Information:**
   - Execute `git fetch origin` to update remote tracking information

3. **Handle Uncommitted Changes:**
   - IF uncommitted changes exist: stage, commit, push, merge, and delete feature branch
   - Think twice before acting - verify branch names and merge targets

4. **Synchronize Local with Remote:**
   - IF local branch is behind remote: execute `git pull origin <branch>`
   - IF merge conflicts detected: HALT immediately and prompt user

5. **Create Feature Branch:**
   - Execute `git checkout -b <descriptive-branch-name>`
   - Use clear, descriptive branch names matching task number

6. **Verify Clean State:**
   - Execute `git status` to confirm clean working directory
   - Proceed to Step 2 only after verification

---

## Step 2: Comprehensive and Extremely Deep Codebase Analysis

**Purpose:** Capture every single detail of the codebase to ensure absolute alignment with requirements, designs, specifications, and constraints.

**Critical Analysis Areas:**

### Codebase Analysis (Complete Deep Dive):

- Read and understand ALL existing code files
- Read and understand ALL documentations found `docs/*`
- Identify current architecture patterns
- Document existing state machine implementation
- Identify optimization opportunities
- Document all dependencies and libraries
- Understand integration points with main.cpp

### Specification Analysis:

**Requirements:**

- Review ALL functional requirements
- Review ALL safety requirements
- Review ALL hardware I/O requirements
- Review ALL UI/menu requirements
- Review ALL control logic requirements
- Review ALL fault handling requirements

**Design:**

- Review software architecture requirements
- Review state machine design
- Review module decomposition
- Review data structures and memory layout
- Review EEPROM layout

**Constraints:**

- Memory optimization requirements
- Safety rules (absolute)
- Industrial behavior expectations
- Integration with main.cpp requirement

**Phase:**

- Identify current task number and description
- Understand task objectives and deliverables
- Identify dependencies on previous phase
- Understand acceptance criteria

**Analysis Outcome:**

- Complete understanding of what exists
- Complete understanding of what needs to be implemented
- Clear plan for memory-optimized implementation
- Confidence in maintaining consistency with existing code

**Proceed to Step 3 only after completing this comprehensive analysis.**

---

## Step 3: Comprehensive and Extremely Deep Analysis of Previously Implemented Phase (N - 1)

**Purpose:** Understand all previously implemented Phase to ensure consistency, avoid duplication, and maintain architectural patterns.

**Actions:**

1. **Identify Previous Phase:**
   - Review specification document
   - Identify all Phase marked as completed (checked boxes)
   - For current task N, analyze Phase 1 through N-1

2. **Analyze Each Previous Task:**
   - What was implemented?
   - Which files were created/modified?
   - What patterns were established?
   - What memory optimization techniques were used?
   - How was integration with main.cpp achieved?
   - What safety mechanisms were implemented?

3. **Consistency Verification:**
   - Coding style consistency
   - Naming convention consistency
   - State machine pattern consistency
   - Error handling pattern consistency
   - Memory optimization pattern consistency

4. **Gap Analysis:**
   - What functionality is still missing?
   - What dependencies exist for current task?
   - What interfaces need to be maintained?

**Analysis Outcome:**

- Complete understanding of implementation history
- Clear picture of established patterns
- Confidence in maintaining consistency
- Awareness of potential conflicts or duplications

**Proceed to Step 4 only after completing this analysis.**

---

## Step 4: Phase Execution Without Deviation

**Purpose:** Implement the task with absolute adherence to requirements, designs, specifications, and constraints.

### Mandatory Compliance Documents:

#### Requirements Compliance:

- Implement EXACTLY what the task requires
- Do not add features not specified
- Do not omit required features
- Follow ALL safety rules
- Follow ALL memory optimization rules

#### Design Compliance:

- Follow established architecture patterns
- Maintain state machine design
- Follow module decomposition
- Use established data structures
- Follow EEPROM layout design

#### Code Compliance:

- Use clear constants and enums
- Avoid magic numbers
- Include comments for safety-critical logic
- Use PROGMEM for constant data
- Minimize global variables
- Use uint8_t and uint16_t appropriately
- Integrate with main.cpp

**CRITICAL: Each implementation task MUST result in meaningful, visible changes. The user must be able to see that the task has been completed and the system has progressed.**

**Proceed to Step 5 only after completing implementation and verification.**

---

## Step 5: User Review and Feedback Integration

**Purpose:** Request user review of the implementation and apply any required updates or changes.

**Actions:**

1. **Present Implementation:**
   - Summarize what was implemented
   - List all files created/modified
   - Highlight key features and functionality
   - Note any deviations or decisions made (if any)

2. **Request Hardware Testing:**
   - **Instruct user to upload the code to ESP8266 hardware**
   - **Instruct user to test the implemented functionality**
   - **Request feedback on hardware behavior**
   - Ask if the implementation works as expected
   - Inquire about any issues or unexpected behavior

3. **Handle Feedback:**
   - **IF user requests changes:**
     - Document requested changes clearly
     - Implement changes following same protocol (Steps 2-4)
     - Re-request review after changes
     - Repeat until user is satisfied
   - **IF user approves without changes:**
     - Confirm explicit approval to proceed
     - Move to Step 6 for Git operations

4. **Verification Before Proceeding:**
   - Ensure user has explicitly stated approval
   - Confirm no additional changes are needed
   - Get clear go-ahead for Git operations

**Do NOT proceed to Step 6 without explicit user approval.**

---

## Step 6: Post-Git Requirement (After Phase Completion)

**Purpose:** Document the implementation, add, commit, push implementation, checkout, merge, and synchronize between local and remote repositories. Delete related branches after detailed verification.

**Actions:**

0. **Document the implementation:**
   - Document every single detail implemented in `docs/*`
   - Prefix the documentation file name with `phase-<N>-<...>`

1. **Verify Current State:**
   - Execute `git status` to check current branch and all modified/created files
   - Execute `git branch -vv` to display branch tracking information
   - Execute `git fetch origin` to update remote tracking information

2. **Stage and Commit Changes:**
   - Review all changes carefully: `git diff`
   - Stage all changes: `git add .`
   - Verify staged changes: `git status`
   - Commit with descriptive message: `git commit -m "feat: [Phase N] Descriptive Phase title and summary"`
   - Use conventional commit format: `feat:`, `fix:`, `refactor:`, `docs:`, etc.

3. **Push Feature Branch:**
   - Push to remote: `git push origin <feature-branch>`
   - Verify push success
   - Confirm remote branch exists: `git branch -r`

4. **Checkout Base Branch:**
   - Checkout main/master: `git checkout main` (or appropriate base branch)
   - Verify clean state: `git status`
   - Pull latest changes: `git pull origin main`

5. **Merge Feature Branch:**
   - **CRITICAL: Think twice before merging**
   - Verify you are on correct base branch: `git branch`
   - Merge feature branch: `git merge <feature-branch>`
   - **IF merge conflicts occur:** HALT immediately, prompt user to resolve conflicts manually
   - **IF merge successful:** Verify merged changes: `git log --oneline -5`

6. **Push Merged Changes:**
   - Push to remote: `git push origin main`
   - Verify push success
   - Confirm remote is updated: `git log origin/main --oneline -5`

7. **Delete Feature Branch (Local and Remote):**
   - **CRITICAL: Verify merge success before deleting**
   - Confirm feature branch is fully merged: `git branch --merged`
   - Delete local branch: `git branch -d <feature-branch>`
   - Delete remote branch: `git push origin --delete <feature-branch>`
   - Verify deletion: `git branch -a` (feature branch should not appear)

8. **Final Synchronization Verification:**
   - Execute `git status` - should show clean working directory
   - Execute `git branch -vv` - should show main branch in sync with origin
   - Execute `git log --oneline -5` - should show recent commit
   - Confirm local and remote are synchronized

9. **Cleanup Verification:**
   - Verify no orphaned branches: `git branch -a`
   - Verify no uncommitted changes: `git status`
   - Verify correct branch: `git branch` (should be on main)

**Task Completion Confirmed:** All changes committed, pushed, merged, and branches cleaned up.

---

## Protocol Enforcement

**This protocol is MANDATORY for EVERY PHASE.**

- **No shortcuts allowed**
- **No exceptions permitted**
- **All six steps must be completed in order**
- **Each step must be verified before proceeding to next**
- **User approval required before Step 6**

**Failure to follow this protocol will result in:**

- Inconsistent implementations
- Git conflicts and branch issues
- Memory limit violations
- Safety rule violations

**Success in following this protocol ensures:**

- Clean Git history
- Consistent code quality
- Exact alignment with specifications
- Maintainable codebase
- Predictable behavior
- Secure implementation
- Memory-optimized code

---

**REMEMBER**: This protocol is your roadmap to success. Follow it religiously.
