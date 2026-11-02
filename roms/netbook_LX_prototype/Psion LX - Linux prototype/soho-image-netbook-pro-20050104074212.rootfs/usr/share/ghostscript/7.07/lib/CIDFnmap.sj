%!
% Copyright (C) 2001 Taiji Yamada and gs-cjk project
%
% This file is part of GNU Ghostscript.
% 
% GNU Ghostscript is distributed in the hope that it will be useful, but
% WITHOUT ANY WARRANTY.  No author or distributor accepts responsibility
% to anyone for the consequences of using it or for whether it serves any
% particular purpose or works at all, unless he says so in writing. Refer
% to the GNU General Public License for full details.
% 
% Everyone is granted permission to copy, modify and redistribute GNU
% Ghostscript, but only under the conditions described in the GNU General
% Public License.  A copy of this license is supposed to have been given
% to you along with GNU Ghostscript so you can know your rights and
% responsibilities.  It should be in a file named COPYING.  Among other
% things, the copyright notice and this notice must be preserved on all
% copies.

% $Id: CIDFnmap.sj,v 1.3 2002/04/23 19:31:59 easysw Exp $
% CID fontmap for japanese-shift-jis encoding CIDFontName

(ÇlÇrñæí©)				/MS-Mincho	;
/#82l#82r#96#BE#92#A9			/MS-Mincho	;
(ÇlÇrÇoñæí©)				/MS-PMincho	;
/#82l#82r#82o#96#BE#92#A9		/MS-PMincho	;
(ÇlÇr ñæí©)				/MS-Mincho	;
/#82l#82r#20#96#BE#92#A9		/MS-Mincho	;
(ÇlÇr Çoñæí©)				/MS-PMincho	;
/#82l#82r#20#82o#96#BE#92#A9		/MS-PMincho	;

(@ÇlÇrñæí©)				/MS-Mincho	;
/@#82l#82r#96#BE#92#A9			/MS-Mincho	;
(@ÇlÇrÇoñæí©)				/MS-PMincho	;
/@#82l#82r#82o#96#BE#92#A9		/MS-PMincho	;
(@ÇlÇr ñæí©)				/MS-Mincho	;
/@#82l#82r#20#96#BE#92#A9		/MS-Mincho	;
(@ÇlÇr Çoñæí©)				/MS-PMincho	;
/@#82l#82r#20#82o#96#BE#92#A9		/MS-PMincho	;

(ÇlÇrÉSÉVÉbÉN)				/MS-Gothic	;
/#82l#82r#83S#83V#83b#83N		/MS-Gothic	;
(ÇlÇrÇoÉSÉVÉbÉN)			/MS-PGothic	;
/#82l#82r#82o#83S#83V#83b#83N		/MS-PGothic	;
(ÇlÇr ÉSÉVÉbÉN)				/MS-Gothic	;
/#82l#82r#20#83S#83V#83b#83N		/MS-Gothic	;
(ÇlÇr ÇoÉSÉVÉbÉN)			/MS-PGothic	;
/#82l#82r#20#82o#83S#83V#83b#83N	/MS-PGothic	;

(@ÇlÇrÉSÉVÉbÉN)				/MS-Gothic	;
/@#82l#82r#83S#83V#83b#83N		/MS-Gothic	;
(@ÇlÇrÇoÉSÉVÉbÉN)			/MS-PGothic	;
/@#82l#82r#82o#83S#83V#83b#83N		/MS-PGothic	;
(@ÇlÇr ÉSÉVÉbÉN)			/MS-Gothic	;
/@#82l#82r#20#83S#83V#83b#83N		/MS-Gothic	;
(@ÇlÇr ÇoÉSÉVÉbÉN)			/MS-PGothic	;
/@#82l#82r#20#82o#83S#83V#83b#83N	/MS-PGothic	;

(ïΩê¨ñæí© W3)		/HeiseiMin-W3		;
(ïΩê¨ñæí©W3)		/HeiseiMin-W3		;
(ïΩê¨äpÉSÉVÉbÉN W5)	/HeiseiKakuGo-W5	;
(ïΩê¨äpÉSÉVÉbÉNW5)	/HeiseiKakuGo-W5	;
(è¨íÀñæí© R)		/KozMin-Regular		;
(è¨íÀñæí©R)		/KozMin-Regular		;

%
% If you found an error at substituting /Adobe-Identity for a fontname
% in reading a PDF file, and if you are the owner of the actual font
% named as the fontname then you can use such locale-specific fonts by
% adding entry as follows:
%
% But, recent PDF files do not require /Adobe-Identity, locale-specific
% fonts. Thus, you may get worse for adding such entry.
%
%(HGä€∫ﬁºØ∏M-PRO)	(hgrsmp.ttf)	/Identity	;
%(HGê≥û≤èëëÃ-PRO)	(hgrskp.ttf)	/Identity	;
%
% As another choise, following one line acts "a fallback font" for such
% as locale-specific fonts:
%
%/Adobe-Identity	(hgrskp.ttf)	/Identity	;
