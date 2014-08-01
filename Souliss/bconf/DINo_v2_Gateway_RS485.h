/**************************************************************************
	Souliss Home Automation
    Copyright (C) 2013  Veseo

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>.
	
	Originally developed by Dario Di Maio
	
***************************************************************************/
/*!
    \file 
    \ingroup
*/
#ifndef bconf_DINOv2_H
#define bconf_DINOv2_H

// List QuickCfg.h parameters that want to be configured as standard
#define	BOARDTYPE_INSKETCH
#define GATEWAYTYPE_INSKETCH
#define	DYNAMICADDRESSING_INSKETCH
#define	ETHERNETMACRAW_INSKETCH
#define	USARTDRIVER_INSKETCH

#define	QC_BOARDTYPE			0x21
#define	QC_GATEWAYTYPE			0x03
#define DYNAMICADDRESSING  		0x01
#define ETHERNETMACRAW  		0x00
#define	USARTDRIVER				Serial1	
#define	USART_TXENABLE			1
#define	USART_TXENPIN			RS485ENABLE
#endif
