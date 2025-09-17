#include "AsciiManager.h"

AsciiManager* g_asciiManager;

AsciiManager::AsciiManager()
{
  //this->vftable = &vftable; Vtable referenced in ghidra but seems unused so far
  this->vm0 = AnmVm(&this->vm0);
  this->vm0 = AnmVm(&this->vm1);
  memset(this, 0, 0x184bc);
  this->flag = this->flag | 2;
  (this->scale).x = 1.0;
  (this->scale).y = 1.0;
  g_asciiManager = this;
  this->color = 0xffffffff;
  this->idk = 0;
  this->alignmentModeH = 9;
  return;
}
