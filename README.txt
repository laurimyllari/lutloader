
collink examples
----------------

Create a device link for PC level output on T440
$ collink -v -e 7 -E n -n \
	-ir -G -r65 -qh \
	Rec709.icm T440.icc \
	video_ycbcr_2_T440.icc
Apply calibration curves to the device link output table
$ applycal -v T440.cal video_ycbcr_2_T440.icc \
	video_ycbcr_2_T440-cal_applied.icc


OR


Create a device link for PC level output on T440, applying the
calibration curves to 3DLUT
$ collink -v -e 7 -E n -n \
	-ir -G -r65 -qh \
	-a T440.cal Rec709.icm T440.icc \
	video_ycbcr_2_T440-with-cal.icc

