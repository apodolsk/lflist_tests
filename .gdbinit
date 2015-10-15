handle SIGUSR1 nostop noprint pass

define pt
  p (flanchor *)(($arg0).constexp & ~7)
end
