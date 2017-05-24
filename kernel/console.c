
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
			      console.c
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++
						    Forrest Yu, 2005
++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/*
	回车键: 把光标移到第一列
	换行键: 把光标前进到下一行
*/


#include "type.h"
#include "const.h"
#include "protect.h"
#include "string.h"
#include "proc.h"
#include "tty.h"
#include "console.h"
#include "global.h"
#include "keyboard.h"
#include "proto.h"

PRIVATE void set_cursor(unsigned int position);
PRIVATE void set_video_start_addr(u32 addr);
PRIVATE void flush(CONSOLE* p_con);


extern int escCount; //记录按下几次esc键
extern int actionBlock; //查找时是否可以按下普通键

/*======================================================================*
			   init_screen
 *======================================================================*/
PUBLIC void init_screen(TTY* p_tty)
{
	int nr_tty = p_tty - tty_table;
	p_tty->p_console = console_table + nr_tty;

	int v_mem_size = V_MEM_SIZE >> 1;	/* 显存总大小 (in WORD) */

	int con_v_mem_size                   = v_mem_size / NR_CONSOLES;
	p_tty->p_console->original_addr      = nr_tty * con_v_mem_size;
	p_tty->p_console->v_mem_limit        = con_v_mem_size;
	p_tty->p_console->current_start_addr = p_tty->p_console->original_addr;

	/* 默认光标位置在最开始处 */
	p_tty->p_console->cursor = p_tty->p_console->original_addr;

	if (nr_tty == 0) {
		/* 第一个控制台沿用原来的光标位置 */
		p_tty->p_console->cursor = disp_pos / 2;
		disp_pos = 0;
	}
	else {
		out_char(p_tty->p_console, nr_tty + '0');
		out_char(p_tty->p_console, '#');
	}

	set_cursor(p_tty->p_console->cursor);
}


/*======================================================================*
			   is_current_console
*======================================================================*/
PUBLIC int is_current_console(CONSOLE* p_con)
{
	return (p_con == &console_table[nr_current_console]);
}


/*======================================================================*
			   out_char
 *======================================================================*/
PUBLIC void out_char(CONSOLE* p_con, char ch)
{
	u8* p_vmem = (u8*)(V_MEM_BASE + p_con->cursor * 2);

	switch(ch) {
	case '\n':
		if (p_con->cursor < p_con->original_addr +
		    p_con->v_mem_limit - SCREEN_WIDTH) {
			*p_vmem++='\n';  //添加回车的标志
			*p_vmem++='\0';
			p_con->cursor = p_con->original_addr + SCREEN_WIDTH * 
				((p_con->cursor - p_con->original_addr) /
				 SCREEN_WIDTH + 1);
		}
		break;
	case '\b':
		if (p_con->cursor > p_con->original_addr) {
			
			int isPAD_ENTER=0;
			//删除回车
			if (p_con->cursor%SCREEN_WIDTH==0)// 当前为屏幕的整数倍
			{
				/* code */
				p_vmem-=SCREEN_WIDTH*2;
				for (int i = 0; i < SCREEN_WIDTH; ++i)//遍历当前行，寻找回车的位置
				{
					if (*p_vmem=='\n')
					{
						/* code */
						p_con->cursor-=SCREEN_WIDTH-i;
						*p_vmem=' ';
						*(p_vmem+1)=DEFAULT_CHAR_COLOR;
						isPAD_ENTER=1;
						break;
					}
					p_vmem+=2;

				}
			}
			if (!isPAD_ENTER)
			{
				//处理TAB键
				if (*(p_vmem-2)==0)
				{
					/* code */
					for (int i = 0; i < 4; ++i)
					{
						/* code */
						p_vmem-=2;
						p_con->cursor--;
						if (*(p_vmem-2)!=0)
						{
							break;
						}
					}
				}else{  //普通字符
					p_con->cursor--;
					*(p_vmem-2) = ' ';
					*(p_vmem-1) = DEFAULT_CHAR_COLOR;
				}
			}
			
		}
		break;
	case '\t':
		if (p_con->cursor<p_con->original_addr+p_con->v_mem_limit-1)
		{
			/* code */
			int i=0;
			int len=4-(p_con->cursor%4);
			for (i = 0; i < len; ++i)//移动光标到4的整数倍的位置
			{
				/* code */
				*p_vmem=0;
				p_vmem+=2;
			}
			p_con->cursor+=len;
		}
		break;
	default:
		if (p_con->cursor <
		    p_con->original_addr + p_con->v_mem_limit - 1) {
			*p_vmem++ = ch;
			if (escCount==1)
			{
				*p_vmem++ = RED;
			}else{
				*p_vmem++ = DEFAULT_CHAR_COLOR;
			}
			p_con->cursor++;
		}
		break;
	}

	while (p_con->cursor >= p_con->current_start_addr + SCREEN_SIZE) {
		scroll_screen(p_con, SCR_DN);
	}

	flush(p_con);
}

/*======================================================================*
                           flush
*======================================================================*/
PRIVATE void flush(CONSOLE* p_con)
{
        set_cursor(p_con->cursor);
        set_video_start_addr(p_con->current_start_addr);
}

/*======================================================================*
			    set_cursor
 *======================================================================*/
PRIVATE void set_cursor(unsigned int position)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, CURSOR_H);
	out_byte(CRTC_DATA_REG, (position >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, CURSOR_L);
	out_byte(CRTC_DATA_REG, position & 0xFF);
	enable_int();
}

/*======================================================================*
			  set_video_start_addr
 *======================================================================*/
PRIVATE void set_video_start_addr(u32 addr)
{
	disable_int();
	out_byte(CRTC_ADDR_REG, START_ADDR_H);
	out_byte(CRTC_DATA_REG, (addr >> 8) & 0xFF);
	out_byte(CRTC_ADDR_REG, START_ADDR_L);
	out_byte(CRTC_DATA_REG, addr & 0xFF);
	enable_int();
}



/*======================================================================*
			   select_console
 *======================================================================*/
PUBLIC void select_console(int nr_console)	/* 0 ~ (NR_CONSOLES - 1) */
{
	if ((nr_console < 0) || (nr_console >= NR_CONSOLES)) {
		return;
	}

	nr_current_console = nr_console;

	set_cursor(console_table[nr_console].cursor);
	set_video_start_addr(console_table[nr_console].current_start_addr);
}

/*======================================================================*
			   scroll_screen
 *----------------------------------------------------------------------*
 滚屏.
 *----------------------------------------------------------------------*
 direction:
	SCR_UP	: 向上滚屏
	SCR_DN	: 向下滚屏
	其它	: 不做处理
 *======================================================================*/
PUBLIC void scroll_screen(CONSOLE* p_con, int direction)
{
	if (direction == SCR_UP) {
		if (p_con->current_start_addr > p_con->original_addr) {
			p_con->current_start_addr -= SCREEN_WIDTH;
		}
	}
	else if (direction == SCR_DN) {
		if (p_con->current_start_addr + SCREEN_SIZE <
		    p_con->original_addr + p_con->v_mem_limit) {
			p_con->current_start_addr += SCREEN_WIDTH;
		}
	}
	else{
	}

	set_video_start_addr(p_con->current_start_addr);
	set_cursor(p_con->cursor);
}

PUBLIC void clear_screen(CONSOLE* p_con) {
	//将当前屏幕上所有字符设为‘ ’
	u8* p_vmem = (u8*)(V_MEM_BASE + p_con->cursor * 2);
	for (; p_con->cursor > p_con->original_addr; p_con->cursor--) {
		*--p_vmem = DEFAULT_CHAR_COLOR;
		*--p_vmem = ' ';
	}
	escCount=0;
	actionBlock=0;
	flush(p_con);
}


PUBLIC void changeColor(CONSOLE* p_con,unsigned int searchBeginPosition){
	unsigned int position=p_con->original_addr;
	int length=p_con->cursor-searchBeginPosition;

	//修改颜色
	while(position!=searchBeginPosition){
		u8* p_vmem=(u8*)(V_MEM_BASE+position*2);
		int isSame=1;

		for (int i = 0; i < length; ++i,p_vmem+=2){
			if (*p_vmem != *((u8*)( V_MEM_BASE + (searchBeginPosition+i)*2) )){
				isSame=0;
				break;
			}
		}

		if (isSame){
			for (int i = 0; i < length; ++i){
				*(p_vmem-1)=RED;
				p_vmem-=2;
			}
		}

		position++;
	}
}
PUBLIC void exitSearch(CONSOLE* p_con,unsigned int searchBeginPosition){


	//去掉搜索字符
	for (int i = 0; i < p_con->cursor-searchBeginPosition+1; ++i)
	{
		out_char(p_con,'\b');
	}
	//初始化
	escCount=0;
	actionBlock=0;
	//改变为默认颜色
	p_con->cursor=searchBeginPosition;
	for (int i = 0; i < searchBeginPosition-p_con->original_addr; ++i)
	{
		u8* p_vmem=(u8*)(V_MEM_BASE+(p_con->original_addr+i)*2);
		*(p_vmem+1)=DEFAULT_CHAR_COLOR;
		if(*p_vmem=='\n'){
			*(p_vmem+1)='\0';
		}
	}


}