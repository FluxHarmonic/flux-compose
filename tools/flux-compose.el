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

(global-set-key (kbd "C-c r") #'flux-compose-start-repl)
(global-set-key (kbd "C-c C-r") #'recompile)

(provide 'flux-compose)
