((nil . ((geiser-scheme-implementation . guile)
         (compile-command . "guix shell --pure -m manifest.scm -- make -C build")
         (eval . (progn (add-to-list 'load-path (expand-file-name "tools"))
                        (require 'flux-compose))))))
