handle SIGUSR1 nostop noprint pass

define pt
  p (flanchor *)(($arg0).constexp & ~7)
end

define fl
  set $a = (flanchor *)(($arg0).constexp & ~7)
  set $n = (flanchor *)(($a->n).constexp & ~7)
  set $p = (flanchor *)(($a->p).constexp & ~7)
  printf "a:%p n:%p p:%p\n", $a, $n, $p
end
