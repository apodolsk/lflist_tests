handle SIGUSR1 nostop noprint pass

define pt
  p (flanchor *)(($arg0).constexp & ~15)
end

define fl
  set $a = (flanchor *)(($arg0).constexp & ~15)
  set $n = (flanchor *)(($a->n).constexp & ~15)
  set $p = (flanchor *)(($a->p).constexp & ~15)
  printf "a: %p n: %p p: %p\n", $a, $n, $p
end

define ppflx
  # if ($arg0).st
  #   set $str = "COMMIT"
  # else
  #   set $str = "RDY"
  # end
  set $ptr = (flanchor *)(($arg0).constexp & ~15)

  # printf "{%p:%lu:%s, %lu}\n", $ptr, ($arg0).nil, $str, ($arg0).gen
  printf "{%p:%lu:%lu, %lu}\n", $ptr, ($arg0).nil, ($arg0).st, ($arg0).gen
end

define cof
  p ($arg1 *)((uptr) $arg0 - (uptr) &(($arg1 *) 0)->$arg2)
end

define cof_aligned
  p ($arg1 *) ((uptr) $arg0 - (uptr) $arg0 % sizeof($arg1))
end

define flwatch
  watch -l *(flx *) $arg0
