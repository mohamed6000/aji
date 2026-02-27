#ifndef DXERR_INCLUDE_H
#define DXERR_INCLUDE_H

#define DXERROR9(v,n,d) {v, TEXT(n), TEXT(d)},
#define DXERROR9LAST(v,n,d) {v, TEXT(n), TEXT(d)}

const TCHAR * __stdcall DXGetErrorString(unsigned long);
const TCHAR * __stdcall DXGetErrorDescription(unsigned long);

#endif // DXERR_INCLUDE_H