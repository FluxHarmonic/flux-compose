;; -*- lexical-binding: t; -*-

(defun flux-compose-start-repl ()
  (interactive)
  (let ((default-directory (project-root (project-current)))
        (geiser-guile-binary (expand-file-name
                               (concat (project-root (project-current))
                                       "bin/flux-compose"))))
    (call-interactively #'run-geiser)))

(global-set-key (kbd "C-c r") #'flux-compose-start-repl)
