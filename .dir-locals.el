((nil . ((geiser-scheme-implementation . guile)
         (eval . (setq-local geiser-guile-binary
                             (expand-file-name
                               (concat (project-root (project-current))
                                       "bin/flux-compose")))))))
