;; -*- lexical-binding: t; -*-

(defvar flux-compose-current-project nil
  "The current .scm file that will be loaded when the REPL starts")

(defun flux-compose-set-current-project (project-file)
  (interactive "f")
  (message "Set current project: %s" project-file)
  (setq flux-compose-current-project project-file))

(defun flux-compose-start-repl ()
  (interactive)
  (let ((default-directory (project-root (project-current)))
        (geiser-guile-binary (expand-file-name
                              (concat (project-root (project-current))
                                      "build/flux-compose")))
        (geiser-guile-init-file flux-compose-current-project))
    (call-interactively #'run-geiser)))

(defun flux-compose-start-repl-new ()
  (interactive)
  (let ((buffer (get-buffer-create "*flux-compose*")))
    (make-comint-in-buffer "flux-compose"
                           buffer
                           (expand-file-name
                            (concat (project-root (project-current))
                                    "build/flux-compose")))
    (switch-to-buffer buffer)))

(defun flux-compose-eval-top-level ()
  (interactive)
  (let ((input (string-replace "\n"
                               ""
                               (substring-no-properties (thing-at-point 'defun)))))
    (with-current-buffer "*flux-compose*"
      (insert input)
      (comint-send-input))))

(global-set-key (kbd "C-c r") #'flux-compose-start-repl)
(global-set-key (kbd "C-c C-r") #'recompile)

(provide 'flux-compose)
