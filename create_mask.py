import mask

MASK_INTERSECTION_ONLY: bool = True

region_of_interest = mask.Mask(width=1280, height=720, fill=True)

region_of_interest.clear_rectangle(x=384, y=104, width=512, height=512)

print(f"\"x_mask\": [{', '.join(str(value) for value in region_of_interest.x())}],")
print(f"\"y_mask\": [{', '.join(str(value) for value in region_of_interest.y())}],")
print(f"\"mask_intersection_only\": {'true' if MASK_INTERSECTION_ONLY else 'false'}")
region_of_interest.write_pixels_to_png_file(MASK_INTERSECTION_ONLY, "mask.png")
