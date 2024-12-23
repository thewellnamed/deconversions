(in-package :COMMON-LISP-USER)

(defconstant +WORK-DIRECTORY+ "work/")
(defconstant +PARS-EXT+       ".pars")
(defconstant +TAG-EXT+        ".tag")
(defconstant +DIR-EXT+        "_res/")
(defconstant +CRIT-FILE+           "criteria")
(defconstant +CHAIN-FILE+          "chains")
(defconstant +BOUND-FILE+          "bounds")
(defconstant +SEGMENT-FILE+        "segments")

(defun RUN (fname)
  (let*  ((outdir      (concatenate 'string +work-directory+ fname +dir-ext+))
         (parsfname    (concatenate 'string outdir fname +pars-ext+))
         (tagfname     (concatenate 'string outdir fname +tag-ext+))
         (realfname    (concatenate 'string +work-directory+ fname))
         (chainfname   (concatenate 'string outdir +chain-file+))
         (boundfname   (concatenate 'string outdir +bound-file+))
         (segmentfname (concatenate 'string outdir +segment-file+))
         (l         nil)
         (chains    nil))
    (unless (probe-file outdir)
      (run-shell-command (format nil "mkdir ~a" outdir)))
    (unless (probe-file tagfname)
      (run-shell-command (format nil "./make-morph ~a ~a" realfname tagfname)))
    (unless (probe-file parsfname)
      (run-shell-command (format nil "./make-shallow  ~a ~a" tagfname parsfname)))
    (init tagfname)
    (setf l (segment tagfname))
    (setf chains (build-chain parsfname l))
    (build-first-appearence-chain-summary chains l
                                          (number-list
                                           (stat-criterion chains))
                                          fname outdir)
    (graphical-representation chains l tagfname outdir)
    (keep-bound-l boundfname l)
    (keep-chains chainfname *final-chains*)
    (keep-segment-l segmentfname *segment-l*)))
