(require 'mozc)
(require 'mozc-emacs-helper-module)

(defun mozc-emacs-helper-process-start (&optional forcep)
  (when (and mozc-helper-process forcep)
    (mozc-emacs-helper-process-stop))
  (setq mozc-helper-process-message-queue nil)
  (mozc-emacs-helper-recv-greeting)
  (setq mozc-helper-process t))

(defun mozc-emacs-helper-process-stop ()
  (setq mozc-helper-process nil))

(defun mozc-emacs-helper-recv-greeting (&rest args)
  (let ((resp (mozc-emacs-helper-module-recv-greeting)))
    (when (and (listp resp)
	       (cdr (assq 'mozc-emacs-helper resp)))
      (setq mozc-helper-process-version (cdr (assq 'version resp)))
      (setq mozc-config-protobuf (cdr (assq 'config resp)))
      t)))

(defun mozc-emacs-helper-send-sexpr (&rest args)
  (let* ((resp (apply #'mozc-emacs-helper-module-send-sexpr args))
	 (err (mozc-protobuf-get resp 'error)))
    (if err (signal 'mozc-response-error (mapcar 'cdr resp)))
    (setq mozc-helper-process-message-queue
	  (nconc mozc-helper-process-message-queue (list resp)))))

(defun mozc-emacs-helper-recv-sexpr ()
  (let ((response (mozc-emacs-helper-recv-response)))
    (if (not response)
        (progn
          (message "mozc-emacs-helper.el: No response from mozc-emacs-helper")
          'no-data-available)
      response)))

(defun mozc-emacs-helper-recv-response ()
  (if mozc-helper-process-message-queue
      (pop mozc-helper-process-message-queue)))


(advice-add 'mozc-helper-process-start :override #'mozc-emacs-helper-process-start)
(advice-add 'mozc-helper-process-stop :override #'mozc-emacs-helper-process-stop)
(advice-add 'mozc-helper-process-recv-greeting :override #'mozc-emacs-helper-recv-greeting)
(advice-add 'mozc-helper-process-send-sexpr :override #'mozc-emacs-helper-send-sexpr)
(advice-add 'mozc-helper-process-recv-sexpr :override #'mozc-emacs-helper-recv-sexpr)
(advice-add 'mozc-helper-process-recv-response :override #'mozc-emacs-helper-recv-response)

(provide 'mozc-emacs-helper)
